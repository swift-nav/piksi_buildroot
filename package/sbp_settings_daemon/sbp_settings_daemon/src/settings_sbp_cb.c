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
#include <libpiksi/util.h>

#include <libsbp/settings.h>

#include <libsettings/settings.h>
#include <libsettings/settings_util.h>

#include <internal/setting.h>

#include "settings_sbp_cb.h"

static void settings_reply(sbp_tx_ctx_t *tx_ctx,
                           setting_t *sdata,
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

static void settings_register_cb(u16 sender_id, u8 len, u8 *msg, void *ctx)
{
  (void)sender_id;
  settings_reg_res_t res = 0;
  setting_t *setting = NULL;

  const char *section = NULL, *name = NULL, *value = NULL, *type = NULL;
  /* Expect to find at least section, name and value */
  if (settings_parse(msg, len, &section, &name, &value, &type) < SETTINGS_TOKENS_VALUE) {
    piksi_log(LOG_ERR, "Error in settings register request: parse error");
    res = SETTINGS_REG_PARSE_FAILED;
    goto reg_response;
  }

  setting = setting_lookup(section, name);
  /* Only register setting if it doesn't already exist */
  if (setting == NULL) {
    setting = calloc(1, sizeof(*setting));
    strncpy(setting->section, section, sizeof(setting->section));
    strncpy(setting->name, name, sizeof(setting->name));
    strncpy(setting->value, value, sizeof(setting->value));

    if (type != NULL) {
      strncpy(setting->type, type, sizeof(setting->type));
    }

    res = setting_register(setting);
  } else {
    piksi_log(LOG_WARNING,
              "Settings register request: %s.%s already registered",
              setting->section,
              setting->name);
    res = SETTINGS_REG_REGISTERED;
  }

  u8 blen = 0;
  char buf[SETTINGS_BUFLEN] = {0};
reg_response:
  buf[blen++] = res;
  settings_reply(ctx,
                 setting,
                 false,
                 true,
                 SBP_MSG_SETTINGS_REGISTER_RESP,
                 NULL,
                 blen,
                 sizeof(buf));
}

static void settings_write_resp_cb(u16 sender_id, u8 len, u8 *msg, void *ctx)
{
  (void)sender_id;
  (void)ctx;
  msg_settings_write_resp_t *resp = (void *)msg;

  if (resp->status != 0) {
    return;
  }

  const char *section = NULL, *name = NULL, *value = NULL, *type = NULL;
  /* Expect to find at least section, name and value */
  if (settings_parse(resp->setting, len - sizeof(resp->status), &section, &name, &value, &type)
      < SETTINGS_TOKENS_VALUE) {
    piksi_log(LOG_ERR, "Error in settings write reply message: parse error");
    return;
  }

  setting_t *sdata = setting_lookup(section, name);
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

static void settings_read_cb(u16 sender_id, u8 len, u8 *msg, void *ctx)
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

  setting_t *sdata = setting_lookup(section, name);
  if (sdata == NULL) {
    piksi_log(LOG_ERR, "Error in settings read request: setting not found (%s.%s)", section, name);
    return;
  }

  settings_reply(tx_ctx, sdata, false, false, SBP_MSG_SETTINGS_READ_RESP, NULL, 0, 0);
}

static void settings_read_by_index_cb(u16 sender_id, u8 len, u8 *msg, void *ctx)
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
  setting_t *sdata = setting_find_by_index(le16toh(req->index));
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

static void settings_save_cb(u16 sender_id, u8 len, u8 *msg, void *ctx)
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

  uint16_t idx = 0;
  while (true) {
    setting_t *setting = setting_find_by_index(idx);

    if (setting == NULL) {
      break;
    }

    ++idx;

    /* Skip unchanged parameters */
    if (!setting->dirty) {
      continue;
    }

    if ((sec == NULL) || (strcmp(setting->section, sec) != 0)) {
      /* New section, write section header */
      sec = setting->section;
      fprintf(f, "[%s]\n", sec);
    }

    /* Write setting */
    fprintf(f, "%s=%s\n", setting->name, setting->value);
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

static void settings_write_cb(u16 sender_id, u8 len, u8 *msg, void *ctx)
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

  setting_t *sdata = setting_lookup(section, name);
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
