/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libpiksi/logging.h>
#include <libpiksi/min_ini.h>

#include <libsbp/settings.h>

#include <libsettings/settings.h>
#include <libsettings/settings_util.h>

#include "settings.h"

#define SETTINGS_FILE "/persistent/config.ini"
#define BUFSIZE 256

struct setting {
  char section[BUFSIZE];
  char name[BUFSIZE];
  char type[BUFSIZE];
  char value[BUFSIZE];
  struct setting *next;
  bool dirty;
};

static struct setting *settings_head;

/* Register a new setting in our linked list */
void setting_register(struct setting *setting)
{
  struct setting *s;

  if (!settings_head) {
    settings_head = setting;
  } else {
    for (s = settings_head; s->next; s = s->next) {
      if ((strcmp(s->section, setting->section) == 0)
          && (strcmp(s->next->section, setting->section) != 0))
        break;
    }
    setting->next = s->next;
    s->next = setting;
  }

  const char *default_value = "{2F9D26FF-F64C-4F9F-94FE-AE9F57758835}";
  char buf[BUFSIZE];

  ini_gets(setting->section, setting->name, default_value, buf, sizeof(buf), SETTINGS_FILE);

  if (strcmp(buf, default_value) != 0) {
    /* Use value from config file */
    strncpy(setting->value, buf, BUFSIZE);
    setting->dirty = true;
  }
}

/* Lookup setting in our linked list */
static struct setting *settings_lookup(const char *section, const char *setting)
{
  for (struct setting *s = settings_head; s; s = s->next)
    if ((strcmp(s->section, section) == 0) && (strcmp(s->name, setting) == 0)) return s;
  return NULL;
}

/* Format setting into SBP message payload */
static int settings_format_setting(struct setting *s, char *buf, int len, bool type)
{
  int buflen;

  /* build and send reply */
  strncpy(buf, s->section, len);
  buflen = strlen(s->section) + 1;
  strncpy(buf + buflen, s->name, len - buflen);
  buflen += strlen(s->name) + 1;
  strncpy(buf + buflen, s->value, len - buflen);
  buflen += strlen(s->value) + 1;
  if (type && s->type[0]) {
    strncpy(buf + buflen, s->type, len - buflen);
    buflen += strlen(s->type) + 1;
    buf[buflen++] = '\0';
  }

  return buflen;
}

static void setting_register_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;

  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  const char *section = NULL, *name = NULL, *value = NULL, *type = NULL;
  /* Expect to find at least section, name and value */
  if (settings_parse(msg, len, &section, &name, &value, &type) < SETTINGS_TOKENS_VALUE) {
    piksi_log(LOG_WARNING, "Error in register message");
  }

  struct setting *sdata = settings_lookup(section, name);
  /* Only register setting if it doesn't already exist */
  if (sdata == NULL) {
    sdata = calloc(1, sizeof(*sdata));
    strncpy(sdata->section, section, BUFSIZE);
    strncpy(sdata->name, name, BUFSIZE);
    strncpy(sdata->value, value, BUFSIZE);

    if (type != NULL) {
      strncpy(sdata->type, type, BUFSIZE);
    }

    setting_register(sdata);
  } else {
    piksi_log(LOG_WARNING, "Setting %s.%s already registered", sdata->section, sdata->name);
  }

  /* Reply with write message with our value */
  char buf[256];
  size_t rlen = settings_format_setting(sdata, buf, sizeof(buf), false);
  sbp_tx_send_from(tx_ctx, SBP_MSG_SETTINGS_WRITE, rlen, (u8 *)buf, SBP_SENDER_ID);
}

static void settings_write_reply_callback(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  (void)context;
  msg_settings_write_resp_t *msg = (void *)msg_;

  if (msg->status != 0) {
    return;
  }

  const char *section = NULL, *name = NULL, *value = NULL, *type = NULL;
  /* Expect to find at least section, name and value */
  if (settings_parse(msg->setting, len - sizeof(msg->status), &section, &name, &value, &type)
      < SETTINGS_TOKENS_VALUE) {
    piksi_log(LOG_WARNING, "Error in write reply message");
    return;
  }

  struct setting *sdata = settings_lookup(section, name);
  if (sdata == NULL) {
    piksi_log(LOG_WARNING, "Write reply for non-existent setting");
    return;
  }

  if (strcmp(sdata->value, value) == 0) {
    /* Setting unchanged */
    return;
  }

  /* This is an assignment, call notify function */
  strncpy(sdata->value, value, BUFSIZE);
  sdata->dirty = true;

  return;
}

static void settings_read_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  if (sender_id != SBP_SENDER_ID) {
    piksi_log(LOG_WARNING, "Invalid sender");
    return;
  }

  static struct setting *s = NULL;
  const char *section = NULL, *setting = NULL;
  char buf[256];
  u8 buflen;

  if (len == 0) {
    piksi_log(LOG_WARNING, "Error in settings read message: length is zero");
    return;
  }

  if (msg[len - 1] != '\0') {
    piksi_log(LOG_WARNING, "Error in settings read message: null string");
    return;
  }

  /* Extract parameters from message:
   * 2 null terminated strings: section, and setting
   */
  section = (const char *)msg;
  for (int i = 0, tok = 0; i < len; i++) {
    if (msg[i] == '\0') {
      tok++;
      switch (tok) {
      case 1: setting = (const char *)&msg[i + 1]; break;
      case 2:
        if (i == len - 1) break;
      default: piksi_log(LOG_WARNING, "Error in settings read message: parse error"); return;
      }
    }
  }

  s = settings_lookup(section, setting);
  if (s == NULL) {
    piksi_log(LOG_WARNING,
              "Bad settings read request: setting not found (%s.%s)",
              section,
              setting);
    return;
  }

  buflen = settings_format_setting(s, buf, sizeof(buf), true);
  sbp_tx_send(tx_ctx, SBP_MSG_SETTINGS_READ_RESP, buflen, (void *)buf);
  return;
}

static void settings_read_by_index_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  if (sender_id != SBP_SENDER_ID) {
    piksi_log(LOG_WARNING, "Invalid sender");
    return;
  }

  struct setting *s = settings_head;
  char buf[256];
  u8 buflen = 0;

  if (len != 2) {
    piksi_log(LOG_WARNING, "Invalid length for settings read by index!");
    return;
  }
  u16 index = (msg[1] << 8) | msg[0];

  for (int i = 0; (i < index) && s; i++, s = s->next)
    ;

  if (s == NULL) {
    sbp_tx_send(tx_ctx, SBP_MSG_SETTINGS_READ_BY_INDEX_DONE, 0, NULL);
    return;
  }

  /* build and send reply */
  buf[buflen++] = msg[0];
  buf[buflen++] = msg[1];
  buflen += settings_format_setting(s, buf + buflen, sizeof(buf) - buflen, true);
  sbp_tx_send(tx_ctx, SBP_MSG_SETTINGS_READ_BY_INDEX_RESP, buflen, (void *)buf);
}

static void settings_save_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;
  (void)context;
  (void)len;
  (void)msg;

  FILE *f = fopen(SETTINGS_FILE, "w");
  const char *sec = NULL;

  if (f == NULL) {
    piksi_log(LOG_ERR, "Error opening config file!");
    return;
  }

  for (struct setting *s = settings_head; s; s = s->next) {
    /* Skip unchanged parameters */
    if (!s->dirty) continue;

    if ((sec == NULL) || (strcmp(s->section, sec) != 0)) {
      /* New section, write section header */
      sec = s->section;
      fprintf(f, "[%s]\n", sec);
    }

    /* Write setting */
    fprintf(f, "%s=%s\n", s->name, s->value);
  }

  fclose(f);
}

static void settings_write_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;

  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  const char *section = NULL, *name = NULL, *value = NULL, *type = NULL;
  /* Expect to find at least section, name and value */
  if ((settings_parse(msg, len, &section, &name, &value, &type) >= SETTINGS_TOKENS_VALUE)
      && settings_lookup(section, name) != NULL) {
    /* This setting looks good; we'll leave it to the owner to complain if
     * there's a problem with the value. */
    return;
  }

  piksi_log(LOG_ERR, "Setting %s.%s rejected", section, name);

  /* Reply with write response rejecting this setting */
  int buflen = 0;
  u8 buf[BUFSIZE] = {0};
  buf[buflen++] = SETTINGS_WR_SETTING_REJECTED;

  if (section != NULL) {
    strncpy(buf, section, BUFSIZE - buflen);
    buflen += strlen(section) + 1;
  }

  if (name != NULL) {
    strncpy(buf + buflen, name, BUFSIZE - buflen);
    buflen += strlen(name) + 1;
  }

  if (value != NULL) {
    strncpy(buf + buflen, value, BUFSIZE - buflen);
    buflen += strlen(value) + 1;
  }

  sbp_tx_send_from(tx_ctx, SBP_MSG_SETTINGS_WRITE_RESP, buflen, buf, SBP_SENDER_ID);
}

void settings_setup(sbp_rx_ctx_t *rx_ctx, sbp_tx_ctx_t *tx_ctx)
{
  sbp_rx_callback_register(rx_ctx, SBP_MSG_SETTINGS_SAVE, settings_save_callback, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_SETTINGS_WRITE, settings_write_callback, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx,
                           SBP_MSG_SETTINGS_WRITE_RESP,
                           settings_write_reply_callback,
                           tx_ctx,
                           NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_SETTINGS_READ_REQ, settings_read_callback, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx,
                           SBP_MSG_SETTINGS_READ_BY_INDEX_REQ,
                           settings_read_by_index_callback,
                           tx_ctx,
                           NULL);
  sbp_rx_callback_register(rx_ctx,
                           SBP_MSG_SETTINGS_REGISTER,
                           setting_register_callback,
                           tx_ctx,
                           NULL);
}

void settings_reset_defaults(void)
{
  unlink(SETTINGS_FILE);
}
