/*
 * Copyright (C) 2016-2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
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
#include <libpiksi/util.h>

#include <libsbp/settings.h>

#include <libsettings/settings.h>
#include <libsettings/settings_util.h>

#include "settings.h"

#define SETTINGS_FILE "/persistent/config.ini"

struct setting {
  char section[SETTINGS_BUFLEN];
  char name[SETTINGS_BUFLEN];
  char value[SETTINGS_BUFLEN];
  char type[SETTINGS_BUFLEN];
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
  char buf[SETTINGS_BUFLEN] = {0};

  ini_gets(setting->section, setting->name, default_value, buf, sizeof(buf), SETTINGS_FILE);

  if (strcmp(buf, default_value) != 0) {
    /* Use value from config file */
    strncpy(setting->value, buf, sizeof(setting->value));
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

static void settings_reply(sbp_tx_ctx_t *tx_ctx,
                           struct setting *sdata,
                           bool type,
                           bool sbp_sender_id,
                           u16 msg_type,
                           char *buf,
                           u8 offset,
                           size_t blen)
{
  assert(tx_ctx != NULL);
  assert(sdata != NULL);

  char l_buf[SETTINGS_BUFLEN] = {0};
  if (buf == NULL) {
    buf = l_buf;
    blen = sizeof(l_buf);
  }

  int res = settings_format(sdata->section,
                            sdata->name,
                            sdata->value,
                            type ? sdata->type : NULL,
                            buf + offset,
                            blen - offset);

  if (res <= 0) {
    piksi_log(LOG_ERR, "Setting %s.%s reply format failed", sdata->section, sdata->name);
    return;
  }

  if (sbp_sender_id) {
    sbp_tx_send_from(tx_ctx, msg_type, res + offset, (u8 *)buf, SBP_SENDER_ID);
  } else {
    sbp_tx_send(tx_ctx, msg_type, res + offset, (u8 *)buf);
  }
}

static void settings_register_cb(u16 sender_id, u8 len, u8 msg[], void *ctx)
{
  (void)sender_id;

  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)ctx;

  const char *section = NULL, *name = NULL, *value = NULL, *type = NULL;
  /* Expect to find at least section, name and value */
  if (settings_parse(msg, len, &section, &name, &value, &type) < SETTINGS_TOKENS_VALUE) {
    piksi_log(LOG_ERR, "Error in settings register request: parse error");
  }

  struct setting *sdata = settings_lookup(section, name);
  /* Only register setting if it doesn't already exist */
  if (sdata == NULL) {
    sdata = calloc(1, sizeof(*sdata));
    strncpy(sdata->section, section, sizeof(sdata->section));
    strncpy(sdata->name, name, sizeof(sdata->name));
    strncpy(sdata->value, value, sizeof(sdata->value));

    if (type != NULL) {
      strncpy(sdata->type, type, sizeof(sdata->type));
    }

    setting_register(sdata);
  } else {
    piksi_log(LOG_WARNING,
              "Settings register request: %s.%s already registered",
              sdata->section,
              sdata->name);
  }

  /* Reply with write message with our value */
  settings_reply(tx_ctx, sdata, false, true, SBP_MSG_SETTINGS_WRITE, NULL, 0, 0);
}

static void settings_write_resp_cb(u16 sender_id, u8 len, u8 msg_[], void *ctx)
{
  (void)sender_id;
  (void)ctx;
  msg_settings_write_resp_t *msg = (void *)msg_;

  if (msg->status != 0) {
    return;
  }

  const char *section = NULL, *name = NULL, *value = NULL, *type = NULL;
  /* Expect to find at least section, name and value */
  if (settings_parse(msg->setting, len - sizeof(msg->status), &section, &name, &value, &type)
      < SETTINGS_TOKENS_VALUE) {
    piksi_log(LOG_ERR, "Error in settings write reply message: parse error");
    return;
  }

  struct setting *sdata = settings_lookup(section, name);
  if (sdata == NULL) {
    piksi_log(LOG_ERR,
              "Error in settings write reply message: %s.%s not registered",
              section,
              name);
    return;
  }

  if (strcmp(sdata->value, value) == 0) {
    /* Setting unchanged */
    return;
  }

  /* This is an assignment, call notify function */
  strncpy(sdata->value, value, sizeof(sdata->value));
  sdata->dirty = true;

  return;
}

static void settings_read_cb(u16 sender_id, u8 len, u8 msg[], void *ctx)
{
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)ctx;

  if (sender_id != SBP_SENDER_ID) {
    piksi_log(LOG_ERR, "Error in settings read request: invalid sender");
    return;
  }

  /* Expect to find at least section and name */
  const char *section = NULL, *name = NULL;
  if (settings_parse(msg, len, &section, &name, NULL, NULL) < SETTINGS_TOKENS_NAME) {
    piksi_log(LOG_ERR, "Error in settings read request: parse error");
    return;
  }

  struct setting *sdata = settings_lookup(section, name);
  if (sdata == NULL) {
    piksi_log(LOG_ERR, "Error in settings read request: setting not found (%s.%s)", section, name);
    return;
  }

  settings_reply(tx_ctx, sdata, false, false, SBP_MSG_SETTINGS_READ_RESP, NULL, 0, 0);
}

static struct setting *setting_find_by_index(u16 index)
{
  struct setting *sdata = settings_head;
  u16 i = 0;

  while ((i < index) && (sdata != NULL)) {
    sdata = sdata->next;
    i++;
  }

  return sdata;
}

static void settings_read_by_index_cb(u16 sender_id, u8 len, u8 msg[], void *ctx)
{
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)ctx;

  if (sender_id != SBP_SENDER_ID) {
    piksi_log(LOG_ERR, "Error in settings read by index request: invalid sender");
    return;
  }

  if (len != sizeof(msg_settings_read_by_index_req_t)) {
    piksi_log(LOG_ERR, "Error in settings read by index request: malformed message");
    return;
  }

  msg_settings_read_by_index_req_t *req = (msg_settings_read_by_index_req_t *)msg;

  /* 16 bit index expected */
  assert(sizeof(req->index) == sizeof(u16));

  /* SBP is little-endian */
  struct setting *sdata = setting_find_by_index(le16toh(req->index));
  if (sdata == NULL) {
    sbp_tx_send(tx_ctx, SBP_MSG_SETTINGS_READ_BY_INDEX_DONE, 0, NULL);
    return;
  }

  /* build and send reply */
  char buf[SETTINGS_BUFLEN] = {0};
  memcpy(buf, msg, len);
  settings_reply(tx_ctx,
                 sdata,
                 true,
                 false,
                 SBP_MSG_SETTINGS_READ_BY_INDEX_RESP,
                 buf,
                 len,
                 sizeof(buf));
}

static void settings_save_cb(u16 sender_id, u8 len, u8 msg[], void *ctx)
{
  (void)sender_id;
  (void)ctx;
  (void)len;
  (void)msg;

  FILE *f = fopen(SETTINGS_FILE, "w");
  const char *sec = NULL;

  if (f == NULL) {
    piksi_log(LOG_ERR, "Error in settings save request: file open failed");
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

static void settings_write_failed(sbp_tx_ctx_t *tx_ctx,
                                  settings_write_res_t res,
                                  char *msg,
                                  int msg_len)
{
  /* Reply with write response rejecting this setting */
  int blen = 0;
  char buf[SETTINGS_BUFLEN] = {0};
  buf[blen++] = res;

  int to_copy = SWFT_MIN(sizeof(buf) - blen, msg_len);
  memcpy(buf + blen, msg, to_copy);

  sbp_tx_send_from(tx_ctx, SBP_MSG_SETTINGS_WRITE_RESP, blen + to_copy, buf, SBP_SENDER_ID);
}

static void settings_write_cb(u16 sender_id, u8 len, u8 msg[], void *ctx)
{
  (void)sender_id;

  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)ctx;

  const char *section = NULL, *name = NULL, *value = NULL, *type = NULL;
  /* Expect to find at least section, name and value */
  if (settings_parse(msg, len, &section, &name, &value, &type) < SETTINGS_TOKENS_VALUE) {
    piksi_log(LOG_ERR, "Error in settings write request: parse error");
    settings_write_failed(tx_ctx, SETTINGS_WR_PARSE_FAILED, msg, len);
    return;
  }

  struct setting *sdata = settings_lookup(section, name);
  if (sdata == NULL) {
    piksi_log(LOG_ERR, "Error in settings write request: %s.%s not registered", section, name);
    settings_write_failed(tx_ctx, SETTINGS_WR_SETTING_REJECTED, msg, len);
    return;
  }

  /* This setting looks good; we'll leave it to the owner to complain if
   * there's a problem with the value. */
  return;
}

void settings_setup(sbp_rx_ctx_t *rx_ctx, sbp_tx_ctx_t *tx_ctx)
{
  sbp_rx_callback_register(rx_ctx, SBP_MSG_SETTINGS_SAVE, settings_save_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_SETTINGS_WRITE, settings_write_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx,
                           SBP_MSG_SETTINGS_WRITE_RESP,
                           settings_write_resp_cb,
                           tx_ctx,
                           NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_SETTINGS_READ_REQ, settings_read_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx,
                           SBP_MSG_SETTINGS_READ_BY_INDEX_REQ,
                           settings_read_by_index_cb,
                           tx_ctx,
                           NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_SETTINGS_REGISTER, settings_register_cb, tx_ctx, NULL);
}

void settings_reset_defaults(void)
{
  unlink(SETTINGS_FILE);
}
