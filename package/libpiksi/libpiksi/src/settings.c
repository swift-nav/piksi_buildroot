/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file settings.c
 * @brief Implementation of Settings Client APIs
 *
 * The piksi settings daemon acts as the manager for onboard settings
 * registration and read responses for read requests while individual
 * processes are responsible for the ownership of settings values and
 * must respond to write requests with the value of the setting as a
 * response to the question of whether or not a write request was
 * valid and accepted by that process.
 *
 * This module provides a context and APIs for client interactions
 * with the settings manager. Where previously these APIs were intended
 * only for settings owning processes to register settings and respond
 * to write requests, the intention will be to allow a fully formed
 * settings client to be built upon these APIs to include read requests
 * and other client side queries.
 *
 * The high level approach to the client is to hold a list of unique
 * settings that can be configured as owned or non-owned (watch-only)
 * by the process running the client. Each setting which is added to
 * the list will be kept in sync with the settings daemon and/or the
 * owning process via asynchronous messages received in the routed
 * endpoint for the client.
 *
 * Standard usage is as follow, initialize the settings context:
 * \code{.c}
 * // Create the settings context
 * settings_ctx_t *settings_ctx = settings_create();
 * \endcode
 * Add a reader to the main pk_loop (if applicable)
 * \code{.c}
 * // Depending on your implementation this will vary
 * settings_attach(settings_ctx, loop);
 * \endcode
 * For settings owners, a setting is registered as follows:
 * \code{.c}
 * settings_register(settings_ctx, "sample_process", "sample_setting",
                     &sample_setting_data, sizeof(sample_setting_data),
                     SETTINGS_TYPE_BOOL,
                     optional_notify_callback, optional_callback_data);
 * \endcode
 * For a process that is tracking a non-owned setting, the process is similar:
 * \code{.c}
 * settings_add_watch(settings_ctx, "sample_process", "sample_setting",
                      &sample_setting_data, sizeof(sample_setting_data),
                      SETTINGS_TYPE_BOOL,
                      optional_notify_callback, optional_callback_data);
 * \endcode
 * The main difference here is that an owned setting will response to write
 * requests only, while a watch-only setting is updated on write responses
 * to stay in sync with successful updates as reported by settings owners.
 * @version v1.4
 * @date 2018-02-23
 */

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/util.h>
#include <libpiksi/logging.h>
#include <libsbp/settings.h>

#include <libpiksi/settings.h>

#define PUB_ENDPOINT "ipc:///var/run/sockets/settings_client.sub"
#define SUB_ENDPOINT "ipc:///var/run/sockets/settings_client.pub"

#define REGISTER_TIMEOUT_MS 100
#define REGISTER_TRIES 5

#define WATCH_INIT_TIMEOUT_MS 100
#define WATCH_INIT_TRIES 5

#define SBP_PAYLOAD_SIZE_MAX 255

typedef struct {
  pk_endpoint_t *cmd_ept;
  const char* command;
  handle_command_fn handler;
} control_command_t;

typedef int (*to_string_fn)(const void *priv, char *str, int slen,
                            const void *blob, int blen);
typedef bool (*from_string_fn)(const void *priv, void *blob, int blen,
                               const char *str);
typedef int (*format_type_fn)(const void *priv, char *str, int slen);

/**
 * @brief Type Data
 *
 * This structure encapsulates the codec for values of a given type
 * which the settings context uses to build a list of known types
 * that it can support when settings are added to the settings list.
 */
typedef struct type_data_s {
  to_string_fn to_string;
  from_string_fn from_string;
  format_type_fn format_type;
  const void *priv;
  struct type_data_s *next;
} type_data_t;

/**
 * @brief Setting Data
 *
 * This structure holds the information use to serialize settings
 * information into sbp messages, as well as internal flags used
 * to evaluate sbp settings callback behavior managed within the
 * settings context.
 */
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
  bool watchonly;
  struct setting_data_s *next;
} setting_data_t;

/**
 * @brief Registration Helper Struct
 *
 * This helper struct is used watch for async callbacks during the
 * registration/add watch read req phases of setup to allow a
 * synchronous blocking stragety. These are for ephemeral use.
 */
typedef struct {
  bool pending;
  bool match;
  u8 compare_data[SBP_PAYLOAD_SIZE_MAX];
  u8 compare_data_len;
} registration_state_t;

/**
 * @brief Settings Context
 *
 * This is the main context for managing client interactions with
 * the settings manager. It implements the client messaging context
 * as well as the list of types and settings necessary to perform
 * the registration, watching and callback functionality of the client.
 */
struct settings_ctx_s {
  pk_loop_t *loop;
  sbp_pubsub_ctx_t *pubsub_ctx;
  type_data_t *type_data_list;
  setting_data_t *setting_data_list;
  registration_state_t registration_state;
  bool write_callback_registered;
  bool write_resp_callback_registered;
  bool read_resp_callback_registered;
  sbp_msg_callbacks_node_t *read_resp_cb_node;
};

enum {
  SBP_WRITE_STATUS_OK,
  SBP_WRITE_STATUS_VALUE_REJECTED,
  SBP_WRITE_STATUS_SETTING_REJECTED,
};

static const char * const bool_enum_names[] = {"False", "True", NULL};

/* Forward Declarations */
static int settings_register_read_resp_callback(settings_ctx_t *ctx);
static int settings_deregister_read_resp_callback(settings_ctx_t *ctx);

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

/**
 * @brief message_header_get - to allow formatting of identity only
 * @param setting_data: the setting to format
 * @param buf: buffer to hold formatted header string
 * @param blen: length of the destination buffer
 * @return bytes written to the buffer, -1 in case of failure
 */
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

/**
 * @brief message_data_get - formatting of value and type
 * @param setting_data: the setting to format
 * @param buf: buffer to hold formatted data string
 * @param blen: length of the destination buffer
 * @return bytes written to the buffer, -1 in case of failure
 */
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

/**
 * @brief type_data_lookup - retrieves type node from settings context
 * @param ctx: settings context
 * @param type: type struct to be matched
 * @return the setting type entry if a match is found, otherwise NULL
 */
static type_data_t * type_data_lookup(settings_ctx_t *ctx, settings_type_t type)
{
  type_data_t *type_data = ctx->type_data_list;
  for (int i = 0; i < type && type_data != NULL; i++) {
    type_data = type_data->next;
  }
  return type_data;
}

/**
 * @brief setting_data_lookup - retrieves setting node from settings context
 * @param ctx: settings context
 * @param section: setting section string to match
 * @param name: setting name string to match
 * @return the setting type entry if a match is found, otherwise NULL
 */
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

/**
 * @brief compare_init - set up compare structure for synchronous req/reply
 * @param ctx: settings context
 * @param data: formatted settings header string to match with incoming messages
 * @param data_len: length of match string
 */
static void compare_init(settings_ctx_t *ctx, const u8 *data, u8 data_len)
{
  registration_state_t *r = &ctx->registration_state;

  assert(data_len <= sizeof(r->compare_data));

  memcpy(r->compare_data, data, data_len);
  r->compare_data_len = data_len;
  r->match = false;
  r->pending = true;
}

/**
 * @brief compare_check - used by message callbacks to perform comparison
 * @param ctx: settings context
 * @param data: settings message payload string to match with header string
 * @param data_len: length of payload string
 */
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
    sbp_rx_reader_interrupt(sbp_pubsub_rx_ctx_get(ctx->pubsub_ctx));
  }
}

/**
 * @brief compare_deinit - clean up compare structure after transaction
 * @param ctx: settings context
 */
static void compare_deinit(settings_ctx_t *ctx)
{
  registration_state_t *r = &ctx->registration_state;
  r->pending = false;
}

/**
 * @brief compare_match - returns status of current comparison
 * This is used as the value to block on until the comparison has been matched
 * successfully or until (based on implementation) a number of retries or a
 * timeout has expired.
 * @param ctx: settings context
 * @return true if response was matched, false if not response has been received
 */
static bool compare_match(settings_ctx_t *ctx)
{
  registration_state_t *r = &ctx->registration_state;
  return r->match;
}

/**
 * @brief type_register - register type data for reference when adding settings
 * @param ctx: settings context
 * @param to_string: serialization method
 * @param from_string: deserialization method
 * @param format_type: ?
 * @param priv: private data used in ser/des methods
 * @param type: type enum that is used to identify this type
 * @return
 */
static int type_register(settings_ctx_t *ctx, to_string_fn to_string,
                         from_string_fn from_string, format_type_fn format_type,
                         const void *priv, settings_type_t *type)
{
  type_data_t *type_data = (type_data_t *)malloc(sizeof(*type_data));
  if (type_data == NULL) {
    piksi_log(LOG_ERR, "error allocating type data");
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

/**
 * @brief setting_data_members_destroy - deinit for settings data
 * @param setting_data: setting to deinit
 */
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

/**
 * @brief setting_data_list_remove - remove a setting from the settings context
 * @param ctx: settings context
 * @param setting_data: setting to remove
 */
static void setting_data_list_remove(settings_ctx_t *ctx,
                                     setting_data_t **setting_data)
{
  if (ctx->setting_data_list != NULL) {
    setting_data_t *s;
    /* Find element before the one to remove */
    for (s = ctx->setting_data_list; s->next != NULL; s = s->next) {
      if (s->next == *setting_data) {
        setting_data_members_destroy(s->next);
        free(s->next);
        *setting_data = NULL;
        s->next = s->next->next;
      }
    }
  }
}

/**
 * @brief setting_create_setting - factory for new settings
 * @param ctx: settings context
 * @param section: section identifier
 * @param name: setting name
 * @param var: non-owning reference to location the data is stored
 * @param var_len: length of data storage
 * @param type: type identifier
 * @param notify: optional notification callback
 * @param notify_context: optional data reference to pass during notification
 * @param readonly: set to true to disable value updates
 * @param watchonly: set to true to indicate a non-owned setting watch
 * @return the newly created setting, NULL if failed
 */
static setting_data_t * setting_create_setting(settings_ctx_t *ctx, const char *section,
                                               const char *name, void *var, size_t var_len,
                                               settings_type_t type, settings_notify_fn notify,
                                               void *notify_context, bool readonly, bool watchonly)
{
  /* Look up type data */
  type_data_t *type_data = type_data_lookup(ctx, type);
  if (type_data == NULL) {
    piksi_log(LOG_ERR, "invalid type");
    return NULL;
  }

  /* Set up setting data */
  setting_data_t *setting_data = (setting_data_t *)
                                     malloc(sizeof(*setting_data));
  if (setting_data == NULL) {
    piksi_log(LOG_ERR, "error allocating setting data");
    return NULL;
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
    .watchonly = watchonly,
    .next = NULL
  };

  if ((setting_data->section == NULL) ||
      (setting_data->name == NULL) ||
      (setting_data->var_copy == NULL)) {
    piksi_log(LOG_ERR, "error allocating setting data members");
    setting_data_members_destroy(setting_data);
    free(setting_data);
    setting_data = NULL;
  }

  return setting_data;
}

/**
 * @brief setting_perform_request_reply_from
 * Performs a synchronous req/reply transation for the provided
 * message using the compare structure to match the header in callbacks.
 * Uses an explicit sender_id to allow for settings interactions with manager.
 * @param ctx: settings context
 * @param message_type: sbp message to use when sending the message
 * @param message: sbp message payload
 * @param message_length: length of payload
 * @param header_length: length of the substring to match during compare
 * @param timeout_ms: timeout between retries
 * @param retries: number of times to retry the transaction
 * @param sender_id: sender_id to use for outgoing message
 * @return zero on success, -1 the transaction failed to complete
 */
static int setting_perform_request_reply_from(settings_ctx_t *ctx,
                                              u16 message_type,
                                              u8 *message,
                                              u8 message_length,
                                              u8 header_length,
                                              u32 timeout_ms,
                                              u8 retries,
                                              u16 sender_id)
{
  /* Register with daemon */
  compare_init(ctx, message, header_length);

  u8 tries = 0;
  bool success = false;
  do {
    sbp_tx_send_from(sbp_pubsub_tx_ctx_get(ctx->pubsub_ctx),
                     message_type,
                     message_length,
                     message,
                     sender_id);
    pk_loop_run_simple_with_timeout(ctx->loop, timeout_ms);
    success = compare_match(ctx);
  } while (!success && (++tries < retries));

  compare_deinit(ctx);

  if (!success) {
    piksi_log(LOG_ERR, "setting req/reply failed");
    return -1;
  }

  return 0;
}

/**
 * @brief setting_perform_request_reply - same as above but with implicit sender_id
 * @param ctx: settings context
 * @param message_type: sbp message to use when sending the message
 * @param message: sbp message payload
 * @param message_length: length of payload
 * @param header_length: length of the substring to match during compare
 * @param timeout_ms: timeout between retries
 * @param retries: number of times to retry the transaction
 * @return zero on success, -1 the transaction failed to complete
 */
static int setting_perform_request_reply(settings_ctx_t *ctx,
                                         u16 message_type,
                                         u8 *message,
                                         u8 message_length,
                                         u8 header_length,
                                         u16 timeout_ms,
                                         u8 retries)
{
  u16 sender_id = sbp_sender_id_get();
  return setting_perform_request_reply_from(ctx,
                                            message_type,
                                            message,
                                            message_length,
                                            header_length,
                                            timeout_ms,
                                            retries,
                                            sender_id);
}

/**
 * @brief setting_register - perform SBP_MSG_SETTINGS_REGISTER req/reply
 * @param ctx: settings context
 * @param setting_data: setting to register with settings daemon
 * @return zero on success, -1 the transaction failed to complete
 */
static int setting_register(settings_ctx_t *ctx, setting_data_t *setting_data)
{
  /* Build message */
  u8 msg[SBP_PAYLOAD_SIZE_MAX];
  u8 msg_len = 0;
  u8 msg_header_len;
  int l;

  l = message_header_get(setting_data, &msg[msg_len], sizeof(msg) - msg_len);
  if (l < 0) {
    piksi_log(LOG_ERR, "error building settings message");
    return -1;
  }
  msg_len += l;
  msg_header_len = msg_len;

  l = message_data_get(setting_data, &msg[msg_len], sizeof(msg) - msg_len);
  if (l < 0) {
    piksi_log(LOG_ERR, "error building settings message");
    return -1;
  }
  msg_len += l;

  return setting_perform_request_reply(ctx,
                                       SBP_MSG_SETTINGS_REGISTER,
                                       msg,
                                       msg_len,
                                       msg_header_len,
                                       REGISTER_TIMEOUT_MS,
                                       REGISTER_TRIES);
}

/**
 * @brief setting_read_watched_value - perform SBP_MSG_SETTINGS_READ_REQ req/reply
 * @param ctx: setting context
 * @param setting_data: setting to read from settings daemon
 * @return zero on success, -1 the transaction failed to complete
 */
static int setting_read_watched_value(settings_ctx_t *ctx, setting_data_t *setting_data)
{
  int result = 0;
  /* Build message */
  u8 msg[SBP_PAYLOAD_SIZE_MAX];
  u8 msg_len = 0;
  int l;

  if (!setting_data->watchonly) {
    piksi_log(LOG_ERR, "cannot update non-watchonly setting manually");
    return -1;
  }

  l = message_header_get(setting_data, &msg[msg_len], sizeof(msg) - msg_len);
  if (l < 0) {
    piksi_log(LOG_ERR, "error building settings read req message");
    return -1;
  }
  msg_len += l;

  if (settings_register_read_resp_callback(ctx) != 0) {
    piksi_log(LOG_ERR, "error registering settings read callback");
    return -1;
  }

  result =  setting_perform_request_reply_from(ctx,
                                               SBP_MSG_SETTINGS_READ_REQ,
                                               msg,
                                               msg_len,
                                               msg_len,
                                               WATCH_INIT_TIMEOUT_MS,
                                               WATCH_INIT_TRIES,
                                               SBP_SENDER_ID);

  settings_deregister_read_resp_callback(ctx);
  return result;
}

int setting_parse_setting_text(const u8 *msg,
                               u8 msg_n,
                               const char **section,
                               const char **name,
                               const char **value)
{
  const char **result_holders[] = { section, name, value };
  u8 start = 0;
  u8 end = 0;
  for (u8 i = 0; i < sizeof(result_holders) / sizeof(*result_holders); i++) {
    bool found = false;
    *(result_holders[i]) = NULL;
    while (end < msg_n) {
      if (msg[end] == '\0') {
        // don't allow empty strings before the third term
        if (end == start && i < 2) {
          return -1;
        } else {
          *(result_holders[i]) = (const char *)msg + start;
          start = (u8)(end + 1);
          found = true;
        }
      }
      end++;
      if (found) {
        break;
      }
    }
  }
  return 0;
}

/**
 * @brief setting_format_setting - formats a fully formed setting message payload
 * @param setting_data: the setting to format
 * @param buf: buffer to hold formatted setting string
 * @param len: length of the destination buffer
 * @return bytes written to the buffer, -1 in case of failure
 */
static int setting_format_setting(setting_data_t *setting_data, char *buf, int len)
{
  int result = 0;
  int written = 0;

  result = message_header_get(setting_data, buf, len - written);
  if (result < 0) {
    return result;
  }
  written += result;

  result = message_data_get(setting_data, buf + written, len - written);
  if (result < 0) {
    return result;
  }
  written += result;

  return written;
}

/**
 * @brief setting_update_value - process value string and update internal data on success
 * @param setting_data: setting to update
 * @param value: value string to evaluate
 * @param write_result: result to pass to write response
 */
static void setting_update_value(setting_data_t *setting_data, const char *value, u8 *write_result)
{
  if (setting_data->readonly) {
    *write_result = SBP_WRITE_STATUS_VALUE_REJECTED;
  } else {
    *write_result = SBP_WRITE_STATUS_OK;
    /* Store copy and update value */
    memcpy(setting_data->var_copy, setting_data->var, setting_data->var_len);
    if (!setting_data->type_data->from_string(setting_data->type_data->priv,
          setting_data->var,
          setting_data->var_len,
          value)) {
      /* Revert value if conversion fails */
      memcpy(setting_data->var, setting_data->var_copy, setting_data->var_len);
      *write_result = SBP_WRITE_STATUS_VALUE_REJECTED;
    } else if (setting_data->notify != NULL) {
      /* Call notify function */
      if (setting_data->notify(setting_data->notify_context) != 0) {
        if (!setting_data->watchonly) {
          /* Revert value if notify returns error */
          memcpy(setting_data->var, setting_data->var_copy, setting_data->var_len);
          *write_result = SBP_WRITE_STATUS_VALUE_REJECTED;
        }
      }
    }
  }
}

/**
 * @brief setting_send_write_response
 * @param ctx: settings context
 * @param write_response: pre-formatted read response sbp message
 * @param len: length of the message
 * @return zero on success, -1 if message failed to send
 */
static int setting_send_write_response(settings_ctx_t *ctx,
                                       msg_settings_write_resp_t *write_response,
                                       u8 len)
{
  if (sbp_tx_send(sbp_pubsub_tx_ctx_get(ctx->pubsub_ctx),
        SBP_MSG_SETTINGS_WRITE_RESP, len, (u8 *)write_response) != 0) {
    piksi_log(LOG_ERR, "error sending settings write response");
    return -1;
  }
  return 0;
}

/**
 * @brief setting_send_read_response
 * @param ctx: settings context
 * @param read_response: pre-formatted read response sbp message
 * @param len: length of the message
 * @return zero on success, -1 if message failed to send
 */
static int setting_send_read_response(settings_ctx_t *ctx,
                                      msg_settings_read_resp_t *read_response,
                                      u8 len)
{
  if (sbp_tx_send(sbp_pubsub_tx_ctx_get(ctx->pubsub_ctx),
        SBP_MSG_SETTINGS_READ_RESP, len, (u8 *)read_response) != 0) {
    piksi_log(LOG_ERR, "error sending settings read response");
    return -1;
  }
  return 0;
}

/**
 * @brief settings_write_callback - callback for SBP_MSG_SETTINGS_WRITE
 */
static void settings_write_callback(u16 sender_id, u8 len, u8 msg[],
                                    void* context)
{
  settings_ctx_t *ctx = (settings_ctx_t *)context;

  if (sender_id != SBP_SENDER_ID) {
    piksi_log(LOG_WARNING, "invalid sender");
    return;
  }

  /* Check for a response to a pending registration request */
  compare_check(ctx, msg, len);

  /* Extract parameters from message:
   * 3 null terminated strings: section, setting and value
   */
  const char *section = NULL;
  const char *name = NULL;
  const char *value = NULL;
  if (setting_parse_setting_text(msg, len, &section, &name, &value) != 0) {
    piksi_log(LOG_WARNING, "error in settings write message");
    return;
  }

  /* Look up setting data */
  setting_data_t *setting_data = setting_data_lookup(ctx, section, name);
  if (setting_data == NULL) {
    return;
  }

  if (setting_data->watchonly) {
    return;
  }

  u8 write_result = SBP_WRITE_STATUS_OK;
  setting_update_value(setting_data, value, &write_result);

  u8 resp[SBP_PAYLOAD_SIZE_MAX];
  u8 resp_len = 0;
  msg_settings_write_resp_t *write_response = (msg_settings_write_resp_t *)resp;
  write_response->status = write_result;
  resp_len += sizeof(write_response->status);
  int l = setting_format_setting(setting_data,
                                 write_response->setting,
                                 SBP_PAYLOAD_SIZE_MAX - resp_len);
  if (l < 0) {
    return;
  }
  resp_len += l;

  setting_send_write_response(ctx, write_response, resp_len);
}

static int settings_update_watch_only(settings_ctx_t *ctx,
                                       u8 *msg,
                                       u8 len)
{
  /* Extract parameters from message:
   * 3 null terminated strings: section, setting and value
   */
  const char *section = NULL;
  const char *name = NULL;
  const char *value = NULL;
  if (setting_parse_setting_text(msg, len, &section, &name, &value) != 0) {
    piksi_log(LOG_WARNING, "error parsing setting");
    return -1;
  }

  /* Look up setting data */
  setting_data_t *setting_data = setting_data_lookup(ctx, section, name);
  if (setting_data == NULL) {
    return 0;
  }

  if (!setting_data->watchonly) {
    return 0;
  }

  u8 write_result = SBP_WRITE_STATUS_OK;
  setting_update_value(setting_data, value, &write_result);
  if (write_result != SBP_WRITE_STATUS_OK) {
    return -1;
  }
  return 0;
}

/**
 * @brief settings_read_resp_callback - callback for SBP_MSG_SETTINGS_READ_RESP
 */
static void settings_read_resp_callback(u16 sender_id, u8 len, u8 msg[],
                                         void* context)
{
  settings_ctx_t *ctx = (settings_ctx_t *)context;
  msg_settings_read_resp_t *read_response = (msg_settings_read_resp_t *)msg;

  /* Check for a response to a pending request */
  compare_check(ctx, read_response->setting, len);

  if (settings_update_watch_only(ctx,
                                 read_response->setting,
                                 len) != 0) {
    piksi_log(LOG_WARNING, "error in settings read response message");
  }
}

/**
 * @brief settings_write_resp_callback - callback for SBP_MSG_SETTINGS_WRITE_RESP
 */
static void settings_write_resp_callback(u16 sender_id, u8 len, u8 msg[],
                                         void* context)
{
  settings_ctx_t *ctx = (settings_ctx_t *)context;
  msg_settings_write_resp_t *write_response = (msg_settings_write_resp_t *)msg;

  /* Check for a response to a pending request */
  compare_check(ctx, write_response->setting, len - sizeof(write_response->status));

  if (settings_update_watch_only(ctx,
                                 write_response->setting,
                                 len - sizeof(write_response->status)) != 0) {
    piksi_log(LOG_WARNING, "error in settings read response message");
  }
}

/**
 * @brief settings_register_write_callback - register callback for SBP_MSG_SETTINGS_WRITE
 * @param ctx: settings context
 * @return zero on success, -1 if registration failed
 */
static int settings_register_write_callback(settings_ctx_t *ctx)
{
  if (!ctx->write_callback_registered) {
    if (sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(ctx->pubsub_ctx),
                                 SBP_MSG_SETTINGS_WRITE,
                                 settings_write_callback, ctx, NULL) != 0) {
      piksi_log(LOG_ERR, "error registering settings write callback");
      return -1;
    } else {
      ctx->write_callback_registered = true;
    }
  }
  return 0;
}

/**
 * @brief settings_register_read_resp_callback - register callback for SBP_MSG_SETTINGS_READ_RESP
 * @param ctx: settings context
 * @return zero on success, -1 if registration failed
 */
static int settings_register_read_resp_callback(settings_ctx_t *ctx)
{
  if (!ctx->read_resp_callback_registered) {
    if (sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(ctx->pubsub_ctx),
                                 SBP_MSG_SETTINGS_READ_RESP,
                                 settings_read_resp_callback, ctx, &ctx->read_resp_cb_node) != 0) {
      piksi_log(LOG_ERR, "error registering settings read resp callback");
      return -1;
    } else {
      ctx->read_resp_callback_registered = true;
    }
  }
  return 0;
}

/**
 * @brief settings_deregister_read_resp_callback - deregister callback for SBP_MSG_SETTINGS_READ_RESP
 * @param ctx: settings context
 * @return zero on success, -1 if deregistration failed
 */
static int settings_deregister_read_resp_callback(settings_ctx_t *ctx)
{
  if (ctx->read_resp_callback_registered) {
    if (sbp_rx_callback_remove(sbp_pubsub_rx_ctx_get(ctx->pubsub_ctx),
                               &ctx->read_resp_cb_node) != 0) {
      piksi_log(LOG_ERR, "error deregistering settings read resp callback");
      return -1;
    } else {
      ctx->read_resp_callback_registered = false;
    }
  }
  return 0;
}

/**
 * @brief settings_register_write_resp_callback - register callback for SBP_MSG_SETTINGS_READ_RESP
 * @param ctx: settings context
 * @return zero on success, -1 if registration failed
 */
static int settings_register_write_resp_callback(settings_ctx_t *ctx)
{
  if (!ctx->write_resp_callback_registered) {
    if (sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(ctx->pubsub_ctx),
                                 SBP_MSG_SETTINGS_WRITE_RESP,
                                 settings_write_resp_callback, ctx, NULL) != 0) {
      piksi_log(LOG_ERR, "error registering settings write resp callback");
      return -1;
    } else {
      ctx->write_resp_callback_registered = true;
    }
  }
  return 0;
}

/**
 * @brief members_destroy - deinit for settings context members
 * @param ctx: settings context to deinit
 */
static void members_destroy(settings_ctx_t *ctx)
{
  if (ctx->pubsub_ctx != NULL) {
    sbp_pubsub_destroy(&ctx->pubsub_ctx);
  }

  if (ctx->loop != NULL) {
    pk_loop_destroy(&ctx->loop);
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

/**
 * @brief destroy - deinit for settings context
 * @param ctx: settings context to deinit
 */
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
    piksi_log(LOG_ERR, "error allocating context");
    return ctx;
  }

  ctx->loop = NULL;
  ctx->type_data_list = NULL;
  ctx->setting_data_list = NULL;
  ctx->registration_state.pending = false;
  ctx->write_callback_registered = false;
  ctx->write_resp_callback_registered = false;
  ctx->read_resp_callback_registered = false;

  ctx->pubsub_ctx = sbp_pubsub_create(PUB_ENDPOINT, SUB_ENDPOINT);
  if (ctx->pubsub_ctx == NULL) {
    piksi_log(LOG_ERR, "error creating PUBSUB context");
    destroy(&ctx);
    return ctx;
  }

  ctx->loop = pk_loop_create();
  if (ctx->loop == NULL) {
    piksi_log(LOG_ERR, "error creating internal loop");
    destroy(&ctx);
    return ctx;
  }

  if (settings_attach(ctx, ctx->loop) != 0) {
    destroy(&ctx);
    return ctx;
  }

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

  return ctx;
}

void settings_destroy(settings_ctx_t **ctx)
{
  if (ctx == NULL || *ctx == NULL) {
    return;
  }

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

/**
 * @brief settings_add_setting - internal subroutine for handling new settings
 * This method forwards all parameters to the setting factory to create a new
 * settings but also performs the addition of the setting to the settings context
 * internal list and performs either registration of the setting (if owned) or
 * a value update (if watch only) to fully initialize the new setting.
 * @param ctx: settings context
 * @param section: section identifier
 * @param name: setting name
 * @param var: non-owning reference to location the data is stored
 * @param var_len: length of data storage
 * @param type: type identifier
 * @param notify: optional notification callback
 * @param notify_context: optional data reference to pass during notification
 * @param readonly: set to true to disable value updates
 * @param watchonly: set to true to indicate a non-owned setting watch
 * @return zero on success, -1 if the addition of the setting has failed
 */
static int settings_add_setting(settings_ctx_t *ctx,
                                const char *section, const char *name,
                                void *var, size_t var_len, settings_type_t type,
                                settings_notify_fn notify, void *notify_context,
                                bool readonly, bool watchonly)
{
  assert(ctx != NULL);
  assert(section != NULL);
  assert(name != NULL);
  assert(var != NULL);

  if (setting_data_lookup(ctx, section, name) != NULL) {
    piksi_log(LOG_ERR, "setting add failed - duplicate setting");
    return -1;
  }

  setting_data_t *setting_data = setting_create_setting(ctx, section, name,
                                                        var, var_len, type,
                                                        notify, notify_context,
                                                        readonly, watchonly);
  if (setting_data == NULL) {
    piksi_log(LOG_ERR, "error creating setting data");
    return -1;
  }

  /* Add to list */
  setting_data_list_insert(ctx, setting_data);

  if (watchonly) {
    if (settings_register_write_resp_callback(ctx) != 0) {
      piksi_log(LOG_ERR, "error registering settings write resp callback");
    }
    if (setting_read_watched_value(ctx, setting_data) != 0) {
      piksi_log(LOG_ERR, "error reading watched setting to initial value");
    }
  } else {
    if (settings_register_write_callback(ctx) != 0) {
      piksi_log(LOG_ERR, "error registering settings write callback");
    }
    if (setting_register(ctx, setting_data) != 0) {
      piksi_log(LOG_ERR, "error registering setting with settings manager");
      setting_data_list_remove(ctx, &setting_data);
      return -1;
    }
  }
  return 0;
}

int settings_register(settings_ctx_t *ctx, const char *section,
                      const char *name, void *var, size_t var_len,
                      settings_type_t type, settings_notify_fn notify,
                      void *notify_context)
{
  return settings_add_setting(ctx, section, name, var, var_len, type,
                              notify, notify_context, false, false);
}

int settings_register_readonly(settings_ctx_t *ctx, const char *section,
                               const char *name, const void *var,
                               size_t var_len, settings_type_t type)
{
  return settings_add_setting(ctx, section, name, (void *)var, var_len, type,
                             NULL, NULL, true, false);
}

int settings_add_watch(settings_ctx_t *ctx, const char *section,
                       const char *name, void *var, size_t var_len,
                       settings_type_t type, settings_notify_fn notify,
                       void *notify_context)
{
  return settings_add_setting(ctx, section, name, var, var_len, type,
                              notify, notify_context, false, true);
}

int settings_attach(settings_ctx_t *ctx, pk_loop_t *pk_loop)
{
  assert(ctx != NULL);
  assert(pk_loop != NULL);

  return sbp_rx_attach(sbp_pubsub_rx_ctx_get(ctx->pubsub_ctx), pk_loop);
}

static void signal_handler(pk_loop_t *loop, void *handle, void *context)
{
  (void)context;
  int signal_value = pk_loop_get_signal_from_handle(handle);

  piksi_log(LOG_DEBUG, "Caught signal: %d", signal_value);

  pk_loop_stop(loop);
}

static void setup_signal_handlers(pk_loop_t *pk_loop)
{
  if (pk_loop_signal_handler_add(pk_loop, SIGINT, signal_handler, NULL) == NULL) {
    piksi_log(LOG_ERR, "Failed to add SIGINT handler to loop");
  }

  if (pk_loop_signal_handler_add(pk_loop, SIGTERM, signal_handler, NULL) == NULL) {
    piksi_log(LOG_ERR, "Failed to add SIGTERM handler to loop");
  }
}

static int command_receive_callback(const u8 *data, const size_t length, void *context)
{
  u8 *result = (u8 *)context;

  if (length != 1) {
    piksi_log(LOG_WARNING, "command received had invalid length: %u", length);
  } else {
    *result = data[0];
  }

  return 0;
}

static void control_handler(pk_loop_t *loop, void *handle, void *context)
{
  control_command_t* cmd_info = (control_command_t*)context;

  u8 data = 0;
  if (pk_endpoint_receive(cmd_info->cmd_ept, command_receive_callback, &data) != 0) {
    piksi_log(LOG_ERR, "%s: error in %s (%s:%d): %s",
              __FUNCTION__, "pk_endpoint_receive", __FILE__, __LINE__,
              pk_endpoint_strerror());
    return;
  }

  if (data != cmd_info->command[0]) {
    piksi_log(LOG_WARNING, "command had invalid value: %c", data);
    return;
  }

  u8 result = cmd_info->handler() ? 1 : 0;

  pk_endpoint_send(cmd_info->cmd_ept, &result, 1);
}

static bool configure_control_socket(pk_loop_t* loop,
                                     const char* control_socket,
                                     const char* control_socket_file,
                                     const char* control_command,
                                     handle_command_fn do_handle_command,
                                     pk_endpoint_t** rep_socket,
                                     control_command_t** ctrl_command_info)
{
  #define CHECK_PRECONDITION(X) if (!(X)) { \
    piksi_log(LOG_ERR, "Precondition check failed: %s (%s:%d)", \
              __STRING(X), __FILE__, __LINE__);                 \
    return false; }

  CHECK_PRECONDITION(loop != NULL);
  CHECK_PRECONDITION(control_socket != NULL);
  CHECK_PRECONDITION(control_socket_file != NULL);
  CHECK_PRECONDITION(control_command != NULL);
  CHECK_PRECONDITION(control_handler != NULL);
  CHECK_PRECONDITION(do_handle_command != NULL);
  CHECK_PRECONDITION(ctrl_command_info != NULL);

  CHECK_PRECONDITION(rep_socket != NULL);

#undef CHECK_PRECONDITION

  int rc = chmod(control_socket_file, 0777);
  if (rc != 0) {
    piksi_log(LOG_ERR, "Error configuring IPC pipe permissions: %s",
              strerror(errno));
    return false;
  }

  *rep_socket = pk_endpoint_create(control_socket, PK_ENDPOINT_REP);
  if (*rep_socket == NULL) {
    const char* err_msg = pk_endpoint_strerror();
    piksi_log(LOG_ERR, "Error creating IPC control path: %s, error: %s",
              control_socket, err_msg);
    return false;
  }

  *ctrl_command_info = malloc(sizeof(control_command_t));

  **ctrl_command_info = (control_command_t){
    .cmd_ept = *rep_socket,
    .command = control_command,
    .handler = do_handle_command
  };

  if (pk_loop_endpoint_reader_add(loop, *rep_socket, control_handler, *ctrl_command_info) == NULL) {
    piksi_log(LOG_ERR, "Error registering reader for control handler");
    return false;
  }

  return true;
}

bool settings_loop(const char* control_socket,
                   const char* control_socket_file,
                   const char* control_command,
                   register_settings_fn do_settings_register,
                   handle_command_fn do_handle_command)
{
  piksi_log(LOG_INFO, "Starting daemon mode for settings...");

  pk_loop_t *loop = pk_loop_create();
  if (loop == NULL) {
    goto settings_loop_cleanup;
  }
  // Install our own signal handlers
  setup_signal_handlers(loop);

  pk_endpoint_t* rep_socket = NULL;
  control_command_t* cmd_info = NULL;

  bool ret = true;

  /* Set up settings */
  settings_ctx_t *settings_ctx = settings_create();
  if (settings_ctx == NULL) {
    ret = false;
    goto settings_loop_cleanup;
  }

  if (settings_attach(settings_ctx, loop) != 0) {
    ret = false;
    goto settings_loop_cleanup;
  }

  do_settings_register(settings_ctx);

  if (control_socket != NULL) {
    bool control_sock_configured =
      configure_control_socket(loop,
                               control_socket,
                               control_socket_file,
                               control_command,
                               do_handle_command,
                               &rep_socket,
                               &cmd_info);
    if (!control_sock_configured) {
      ret = false;
      goto settings_loop_cleanup;
    }
  }

  pk_loop_run_simple(loop);

settings_loop_cleanup:
  if (rep_socket != NULL)
    pk_endpoint_destroy(&rep_socket);

  if (cmd_info != NULL)
    free(cmd_info);

  settings_destroy(&settings_ctx);
  pk_loop_destroy(&loop);

  return ret;
}

bool settings_loop_simple(register_settings_fn do_settings_register)
{
  return settings_loop(NULL, NULL, NULL, do_settings_register, NULL);
}

int settings_loop_send_command(const char* target_description,
                               const char* command,
                               const char* command_description,
                               const char* control_socket)
{
# define CHECK_PK_EPT_ERR(COND, FUNC) \
  if (COND) { \
    piksi_log(LOG_ERR, "%s: error in %s (%s:%d): %s", \
              __FUNCTION__, __STRING(FUNC), __FILE__, __LINE__, \
              pk_endpoint_strerror()); \
    return -1; \
  }

#define CMD_INFO_MSG "Sending '%s' command to %s..."

  piksi_log(LOG_INFO, CMD_INFO_MSG, command_description, target_description);
  printf(CMD_INFO_MSG "\n", command_description, target_description);

  pk_endpoint_t* req_socket = pk_endpoint_create(control_socket, PK_ENDPOINT_REQ);
  CHECK_PK_EPT_ERR(req_socket == NULL, pk_endpoint_create);

  int ret = pk_endpoint_send(req_socket, command, strlen(command));
  CHECK_PK_EPT_ERR(ret != 0, pk_endpoint_send);

  u8 result = 0;
  ret = pk_endpoint_receive(req_socket, command_receive_callback, &result);
  CHECK_PK_EPT_ERR(ret != 0, pk_endpoint_receive);

# define CMD_RESULT_MSG "Result of '%s' command: %hhu"

  piksi_log(LOG_INFO, CMD_RESULT_MSG, command_description, result);
  printf(CMD_RESULT_MSG "\n", command_description, result);

  pk_endpoint_destroy(&req_socket);

  return 0;

# undef CMD_RESULT_MSG
# undef CHECK_PK_EPT_ERR
# undef CMD_INFO_MSG
}

