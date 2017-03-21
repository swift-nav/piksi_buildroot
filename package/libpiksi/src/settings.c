/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "settings.h"
#include "sbp_zmq_pubsub.h"
#include "util.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <libsbp/settings.h>

#define PUB_ENDPOINT ">tcp://127.0.0.1:43071"
#define SUB_ENDPOINT ">tcp://127.0.0.1:43070"

#define REGISTER_TIMEOUT_ms 100
#define REGISTER_TRIES 5

#define SBP_PAYLOAD_SIZE_MAX 255

typedef int (*to_string_fn)(const void *priv, char *str, int slen,
                            const void *blob, int blen);
typedef bool (*from_string_fn)(const void *priv, void *blob, int blen,
                               const char *str);
typedef int (*format_type_fn)(const void *priv, char *str, int slen);

typedef struct type_data_s {
  to_string_fn to_string;
  from_string_fn from_string;
  format_type_fn format_type;
  const void *priv;
  struct type_data_s *next;
} type_data_t;

typedef struct setting_data_s {
  char *section;
  char *name;
  void *var;
  size_t var_len;
  void *var_copy;
  type_data_t *type_data;
  settings_notify_fn notify;
  void *notify_context;
  bool readonly;
  struct setting_data_s *next;
} setting_data_t;

typedef struct {
  bool pending;
  bool match;
  u8 compare_data[SBP_PAYLOAD_SIZE_MAX];
  u8 compare_data_len;
} registration_state_t;

struct settings_ctx_s {
  sbp_zmq_pubsub_ctx_t *pubsub_ctx;
  type_data_t *type_data_list;
  setting_data_t *setting_data_list;
  registration_state_t registration_state;
};

static const char * const bool_enum_names[] = {"False", "True", NULL};

static int float_to_string(const void *priv, char *str, int slen,
                           const void *blob, int blen)
{
  (void)priv;

  switch (blen) {
  case 4:
    return snprintf(str, slen, "%.12g", (double)*(float*)blob);
  case 8:
    return snprintf(str, slen, "%.12g", *(double*)blob);
  }
  return -1;
}

static bool float_from_string(const void *priv, void *blob, int blen,
                              const char *str)
{
  (void)priv;

  switch (blen) {
  case 4:
    return sscanf(str, "%g", (float*)blob) == 1;
  case 8:
    return sscanf(str, "%lg", (double*)blob) == 1;
  }
  return false;
}

static int int_to_string(const void *priv, char *str, int slen,
                         const void *blob, int blen)
{
  (void)priv;

  switch (blen) {
  case 1:
    return snprintf(str, slen, "%hhd", *(s8*)blob);
  case 2:
    return snprintf(str, slen, "%hd", *(s16*)blob);
  case 4:
    return snprintf(str, slen, "%ld", *(s32*)blob);
  }
  return -1;
}

static bool int_from_string(const void *priv, void *blob, int blen,
                            const char *str)
{
  (void)priv;

  switch (blen) {
  case 1: {
    s16 tmp;
    /* Newlib's crappy sscanf doesn't understand %hhd */
    if (sscanf(str, "%hd", &tmp) == 1) {
      *(s8*)blob = tmp;
      return true;
    }
    return false;
  }
  case 2:
    return sscanf(str, "%hd", (s16*)blob) == 1;
  case 4:
    return sscanf(str, "%ld", (s32*)blob) == 1;
  }
  return false;
}

static int str_to_string(const void *priv, char *str, int slen,
                         const void *blob, int blen)
{
  (void)priv;
  (void)blen;

  return snprintf(str, slen, "%s", blob);
}

static bool str_from_string(const void *priv, void *blob, int blen,
                            const char *str)
{
  (void)priv;

  int l = snprintf(blob, blen, "%s", str);
  if ((l < 0) || (l >= blen)) {
    return false;
  }

  return true;
}

static int enum_to_string(const void *priv, char *str, int slen,
                          const void *blob, int blen)
{
  (void)blen;

  const char * const *enum_names = priv;
  int index = *(u8*)blob;
  return snprintf(str, slen, "%s", enum_names[index]);
}

static bool enum_from_string(const void *priv, void *blob, int blen,
                             const char *str)
{
  (void)blen;

  const char * const *enum_names = priv;
  int i;

  for (i = 0; enum_names[i] && (strcmp(str, enum_names[i]) != 0); i++) {
    ;
  }

  if (!enum_names[i]) {
    return false;
  }

  *(u8*)blob = i;

  return true;
}

static int enum_format_type(const void *priv, char *str, int slen)
{
  int n = 0;
  int l;

  /* Print "enum:" header */
  l = snprintf(&str[n], slen - n, "enum:");
  if (l < 0) {
    return l;
  }
  n += l;

  /* Print enum names separated by commas */
  for (const char * const *enum_names = priv; *enum_names; enum_names++) {
    if (n < slen) {
      l = snprintf(&str[n], slen - n, "%s,", *enum_names);
      if (l < 0) {
        return l;
      }
      n += l;
    } else {
      n += strlen(*enum_names) + 1;
    }
  }

  /* Replace last comma with NULL */
  if ((n > 0) && (n - 1 < slen)) {
    str[n - 1] = '\0';
    n--;
  }

  return n;
}

static int message_header_get(setting_data_t *setting_data, char *buf, int blen)
{
  int n = 0;
  int l;

  /* Section */
  l = snprintf(&buf[n], blen - n, "%s", setting_data->section);
  if ((l < 0) || (l >= blen - n)) {
    return -1;
  }
  n += l + 1;

  /* Name */
  l = snprintf(&buf[n], blen - n, "%s", setting_data->name);
  if ((l < 0) || (l >= blen - n)) {
    return -1;
  }
  n += l + 1;

  return n;
}

static int message_data_get(setting_data_t *setting_data, char *buf, int blen)
{
  int n = 0;
  int l;

  /* Value */
  l = setting_data->type_data->to_string(setting_data->type_data->priv,
                                         &buf[n], blen - n, setting_data->var,
                                         setting_data->var_len);
  if ((l < 0) || (l >= blen - n)) {
    return -1;
  }
  n += l + 1;

  /* Type information */
  if (setting_data->type_data->format_type != NULL) {
    l = setting_data->type_data->format_type(setting_data->type_data->priv,
                                             &buf[n], blen - n);
    if ((l < 0) || (l >= blen - n)) {
      return -1;
    }
    n += l + 1;
  }

  return n;
}

static type_data_t * type_data_lookup(settings_ctx_t *ctx, settings_type_t type)
{
  type_data_t *type_data = ctx->type_data_list;
  for (int i = 0; i < type && type_data != NULL; i++) {
    type_data = type_data->next;
  }
  return type_data;
}

static setting_data_t * setting_data_lookup(settings_ctx_t *ctx,
                                            const char *section,
                                            const char *name)
{
  setting_data_t *setting_data = ctx->setting_data_list;
  while (setting_data != NULL) {
    if ((strcmp(setting_data->section, section) == 0) &&
        (strcmp(setting_data->name, name) == 0)) {
      break;
    }
    setting_data = setting_data->next;
  }
  return setting_data;
}

static void setting_data_list_insert(settings_ctx_t *ctx,
                                     setting_data_t *setting_data)
{
  if (ctx->setting_data_list == NULL) {
    ctx->setting_data_list = setting_data;
  } else {
    setting_data_t *s;
    /* Find last element in the same section */
    for (s = ctx->setting_data_list; s->next != NULL; s = s->next) {
      if ((strcmp(s->section, setting_data->section) == 0) &&
          (strcmp(s->next->section, setting_data->section) != 0)) {
        break;
      }
    }
    setting_data->next = s->next;
    s->next = setting_data;
  }
}

static void compare_init(settings_ctx_t *ctx, const u8 *data, u8 data_len)
{
  registration_state_t *r = &ctx->registration_state;

  assert(data_len <= sizeof(r->compare_data));

  memcpy(r->compare_data, data, data_len);
  r->compare_data_len = data_len;
  r->match = false;
  r->pending = true;
}

static void compare_check(settings_ctx_t *ctx, const u8 *data, u8 data_len)
{
  registration_state_t *r = &ctx->registration_state;

  if (!r->pending) {
    return;
  }

  if ((data_len >= r->compare_data_len) &&
      (memcmp(data, r->compare_data, r->compare_data_len) == 0)) {
    r->match = true;
    r->pending = false;
    sbp_zmq_rx_reader_interrupt(sbp_zmq_pubsub_rx_ctx_get(ctx->pubsub_ctx));
  }
}

static void compare_deinit(settings_ctx_t *ctx)
{
  registration_state_t *r = &ctx->registration_state;
  r->pending = false;
}

static bool compare_match(settings_ctx_t *ctx)
{
  registration_state_t *r = &ctx->registration_state;
  return r->match;
}

static int type_register(settings_ctx_t *ctx, to_string_fn to_string,
                         from_string_fn from_string, format_type_fn format_type,
                         const void *priv, settings_type_t *type)
{
  type_data_t *type_data = (type_data_t *)malloc(sizeof(*type_data));
  if (type_data == NULL) {
    printf("error allocating type data\n");
    return -1;
  }

  *type_data = (type_data_t) {
    .to_string = to_string,
    .from_string = from_string,
    .format_type = format_type,
    .priv = priv,
    .next = NULL
  };

  /* Add to list */
  settings_type_t next_type = 0;
  type_data_t **p_next = &ctx->type_data_list;
  while (*p_next != NULL) {
    p_next = &(*p_next)->next;
    next_type++;
  }

  *p_next = type_data;
  *type = next_type;
  return 0;
}

static void setting_data_members_destroy(setting_data_t *setting_data)
{
  if (setting_data->section) {
    free(setting_data->section);
    setting_data->section = NULL;
  }

  if (setting_data->name) {
    free(setting_data->name);
    setting_data->name = NULL;
  }

  if (setting_data->var_copy) {
    free(setting_data->var_copy);
    setting_data->var_copy = NULL;
  }
}

static int setting_register(settings_ctx_t *ctx, const char *section,
                            const char *name, void *var, size_t var_len,
                            settings_type_t type, settings_notify_fn notify,
                            void *notify_context, bool readonly)
{
  /* Look up type data */
  type_data_t *type_data = type_data_lookup(ctx, type);
  if (type_data == NULL) {
    printf("invalid type\n");
    return -1;
  }

  /* Set up setting data */
  setting_data_t *setting_data = (setting_data_t *)
                                     malloc(sizeof(*setting_data));
  if (setting_data == NULL) {
    printf("error allocating setting data\n");
    return -1;
  }

  *setting_data = (setting_data_t) {
    .section = strdup(section),
    .name = strdup(name),
    .var = var,
    .var_len = var_len,
    .var_copy = malloc(var_len),
    .type_data = type_data,
    .notify = notify,
    .notify_context = notify_context,
    .readonly = readonly,
    .next = NULL
  };

  if ((setting_data->section == NULL) ||
      (setting_data->name == NULL) ||
      (setting_data->var_copy == NULL)) {
    printf("error allocating setting data members");
    setting_data_members_destroy(setting_data);
    free(setting_data);
    setting_data = NULL;
    return -1;
  }

  /* Add to list */
  setting_data_list_insert(ctx, setting_data);

  /* Build message */
  u8 msg[SBP_PAYLOAD_SIZE_MAX];
  u8 msg_len = 0;
  u8 msg_header_len;
  int l;

  l = message_header_get(setting_data, &msg[msg_len], sizeof(msg) - msg_len);
  if (l < 0) {
    printf("error building settings message\n");
    return -1;
  }
  msg_len += l;
  msg_header_len = msg_len;

  l = message_data_get(setting_data, &msg[msg_len], sizeof(msg) - msg_len);
  if (l < 0) {
    printf("error building settings message\n");
    return -1;
  }
  msg_len += l;

  /* Register with daemon */
  compare_init(ctx, msg, msg_header_len);

  u8 tries = 0;
  bool success = false;
  do {
    sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(ctx->pubsub_ctx),
                    SBP_MSG_SETTINGS_REGISTER, msg_len, msg);
    zmq_simple_loop_timeout(sbp_zmq_pubsub_zloop_get(ctx->pubsub_ctx),
                            REGISTER_TIMEOUT_ms);
    success = compare_match(ctx);

  } while (!success && (++tries < REGISTER_TRIES));

  compare_deinit(ctx);

  if (!success) {
    printf("setting registration failed\n");
    return -1;
  }

  return 0;
}

static void settings_write_callback(u16 sender_id, u8 len, u8 msg[],
                                    void* context)
{
  settings_ctx_t *ctx = (settings_ctx_t *)context;

  if (sender_id != SBP_SENDER_ID) {
    printf("invalid sender\n");
    return;
  }

  /* Check for a response to a pending registration request */
  compare_check(ctx, msg, len);

  if ((len == 0) ||
      (msg[len-1] != '\0')) {
    printf("error in settings write message\n");
    return;
  }

  /* Extract parameters from message:
   * 3 null terminated strings: section, setting and value
   */
  const char *section = NULL;
  const char *name = NULL;
  const char *value = NULL;
  section = (const char *)msg;
  for (int i = 0, tok = 0; i < len; i++) {
    if (msg[i] == '\0') {
      tok++;
      switch (tok) {
      case 1:
        name = (const char *)&msg[i+1];
        break;
      case 2:
        if (i + 1 < len)
          value = (const char *)&msg[i+1];
        break;
      case 3:
        if (i == len-1)
          break;
      default:
        printf("error in settings write message\n");
        return;
      }
    }
  }

  if (value == NULL) {
    printf("error in settings write message\n");
    return;
  }

  /* Look up setting data */
  setting_data_t *setting_data = setting_data_lookup(ctx, section, name);
  if (setting_data == NULL) {
    return;
  }

  if (!setting_data->readonly) {
    /* Store copy and update value */
    memcpy(setting_data->var_copy, setting_data->var, setting_data->var_len);
    if (!setting_data->type_data->from_string(setting_data->type_data->priv,
                                             setting_data->var,
                                             setting_data->var_len,
                                             value)) {
      /* Revert value if conversion fails */
      memcpy(setting_data->var, setting_data->var_copy, setting_data->var_len);
    } else if (setting_data->notify != NULL) {
      /* Call notify function */
      if (setting_data->notify(setting_data->notify_context) != 0) {
        /* Revert value if notify returns error */
        memcpy(setting_data->var, setting_data->var_copy, setting_data->var_len);
      }
    }
  }

  /* Build message */
  u8 resp[SBP_PAYLOAD_SIZE_MAX];
  u8 resp_len = 0;
  int l;

  l = message_header_get(setting_data, &resp[resp_len], sizeof(resp) - resp_len);
  if (l < 0) {
    printf("error building settings message\n");
    return;
  }
  resp_len += l;

  l = message_data_get(setting_data, &resp[resp_len], sizeof(resp) - resp_len);
  if (l < 0) {
    printf("error building settings message\n");
    return;
  }
  resp_len += l;

  if (sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(ctx->pubsub_ctx),
                      SBP_MSG_SETTINGS_READ_RESP, resp_len, resp) != 0) {
    printf("error sending settings read response\n");
    return;
  }
}

static void members_destroy(settings_ctx_t *ctx)
{
  if (ctx->pubsub_ctx != NULL) {
    sbp_zmq_pubsub_destroy(&ctx->pubsub_ctx);
  }

  /* Free type data list elements */
  while (ctx->type_data_list != NULL) {
    type_data_t *t = ctx->type_data_list;
    ctx->type_data_list = ctx->type_data_list->next;
    free(t);
  }

  /* Free setting data list elements */
  while (ctx->setting_data_list != NULL) {
    setting_data_t *s = ctx->setting_data_list;
    ctx->setting_data_list = ctx->setting_data_list->next;
    setting_data_members_destroy(s);
    free(s);
  }
}

static void destroy(settings_ctx_t **ctx)
{
  members_destroy(*ctx);
  free(*ctx);
  *ctx = NULL;
}

settings_ctx_t * settings_create(void)
{
  settings_ctx_t *ctx = (settings_ctx_t *)malloc(sizeof(*ctx));
  if (ctx == NULL) {
    printf("error allocating context\n");
    return ctx;
  }

  ctx->pubsub_ctx = sbp_zmq_pubsub_create(PUB_ENDPOINT, SUB_ENDPOINT);
  if (ctx->pubsub_ctx == NULL) {
    printf("error creating PUBSUB context\n");
    destroy(&ctx);
    return ctx;
  }

  ctx->type_data_list = NULL;
  ctx->setting_data_list = NULL;
  ctx->registration_state.pending = false;

  /* Register standard types */
  settings_type_t type;

  if (type_register(ctx, int_to_string, int_from_string, NULL,
                    NULL, &type) != 0) {
    destroy(&ctx);
    return ctx;
  }
  assert(type == SETTINGS_TYPE_INT);

  if (type_register(ctx, float_to_string, float_from_string, NULL,
                    NULL, &type) != 0) {
    destroy(&ctx);
    return ctx;
  }
  assert(type == SETTINGS_TYPE_FLOAT);

  if (type_register(ctx, str_to_string, str_from_string, NULL,
                    NULL, &type) != 0) {
    destroy(&ctx);
    return ctx;
  }
  assert(type == SETTINGS_TYPE_STRING);

  if (type_register(ctx, enum_to_string, enum_from_string, enum_format_type,
                    bool_enum_names, &type) != 0) {
    destroy(&ctx);
    return ctx;
  }
  assert(type == SETTINGS_TYPE_BOOL);

  if (sbp_zmq_rx_callback_register(sbp_zmq_pubsub_rx_ctx_get(ctx->pubsub_ctx),
                                   SBP_MSG_SETTINGS_WRITE,
                                   settings_write_callback, ctx, NULL) != 0) {
    printf("error registering settings write callback\n");
    destroy(&ctx);
    return ctx;
  }

  return ctx;
}

void settings_destroy(settings_ctx_t **ctx)
{
  assert(ctx != NULL);
  assert(*ctx != NULL);

  destroy(ctx);
}

int settings_type_register_enum(settings_ctx_t *ctx,
                                const char * const enum_names[],
                                settings_type_t *type)
{
  assert(ctx != NULL);
  assert(enum_names != NULL);
  assert(type != NULL);

  return type_register(ctx, enum_to_string, enum_from_string, enum_format_type,
                       enum_names, type);
}

int settings_register(settings_ctx_t *ctx, const char *section,
                      const char *name, void *var, size_t var_len,
                      settings_type_t type, settings_notify_fn notify,
                      void *notify_context)
{
  assert(ctx != NULL);
  assert(section != NULL);
  assert(name != NULL);
  assert(var != NULL);

  return setting_register(ctx, section, name, var, var_len, type,
                          notify, notify_context, false);
}

int settings_register_readonly(settings_ctx_t *ctx, const char *section,
                               const char *name, const void *var,
                               size_t var_len, settings_type_t type)
{
  assert(ctx != NULL);
  assert(section != NULL);
  assert(name != NULL);
  assert(var != NULL);

  return setting_register(ctx, section, name, (void *)var, var_len, type,
                          NULL, NULL, true);
}

int settings_read(settings_ctx_t *ctx)
{
  assert(ctx != NULL);

  return sbp_zmq_rx_read(sbp_zmq_pubsub_rx_ctx_get(ctx->pubsub_ctx));
}

int settings_pollitem_init(settings_ctx_t *ctx, zmq_pollitem_t *pollitem)
{
  assert(ctx != NULL);
  assert(pollitem != NULL);

  return sbp_zmq_rx_pollitem_init(sbp_zmq_pubsub_rx_ctx_get(ctx->pubsub_ctx),
                                  pollitem);
}

int settings_pollitem_check(settings_ctx_t *ctx, zmq_pollitem_t *pollitem)
{
  assert(ctx != NULL);
  assert(pollitem != NULL);

  return sbp_zmq_rx_pollitem_check(sbp_zmq_pubsub_rx_ctx_get(ctx->pubsub_ctx),
                                   pollitem);
}

int settings_reader_add(settings_ctx_t *ctx, zloop_t *zloop)
{
  assert(ctx != NULL);
  assert(zloop != NULL);

  return sbp_zmq_rx_reader_add(sbp_zmq_pubsub_rx_ctx_get(ctx->pubsub_ctx),
                               zloop);
}

int settings_reader_remove(settings_ctx_t *ctx, zloop_t *zloop)
{
  assert(ctx != NULL);
  assert(zloop != NULL);

  return sbp_zmq_rx_reader_remove(sbp_zmq_pubsub_rx_ctx_get(ctx->pubsub_ctx),
                                  zloop);
}
