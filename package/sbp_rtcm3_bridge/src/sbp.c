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

#include "sbp.h"

#include <libpiksi/logging.h>
#include <libsbp/piksi.h>

static struct {
  sbp_zmq_rx_ctx_t *rx_ctx;
  sbp_zmq_tx_ctx_t *tx_ctx;
} ctx = {
  .rx_ctx = NULL,
  .tx_ctx = NULL
};

int sbp_init(sbp_zmq_rx_ctx_t *rx_ctx, sbp_zmq_tx_ctx_t *tx_ctx)
{
  if(rx_ctx == NULL || tx_ctx == NULL){
    return -1;
  }
  ctx.rx_ctx = rx_ctx;
  ctx.tx_ctx = tx_ctx;
  return 0;
}

void sbp_message_send(u16 msg_type, u8 len, u8 *payload, u16 sender_id)
{
  if (ctx.tx_ctx == NULL) {
    return;
  }

  sbp_zmq_tx_send_from(ctx.tx_ctx, msg_type, len, payload, sender_id);
}

int sbp_callback_register(u16 msg_type, sbp_msg_callback_t cb, void *context)
{
  if (ctx.rx_ctx == NULL) {
    return -1;
  }

  return sbp_zmq_rx_callback_register(ctx.rx_ctx, msg_type, cb, context, NULL);
}

/* this is a short term solution - settings refactor to remove */
static int parse_setting_read_resp(const u8 *msg,
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
        if (end == start) {
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

static const char *const bool_names[] = { "False", "True" };
static const char *const section_simulator = "simulator";
static const char *const name_enabled = "enabled";

static int check_simulator_enabled(const char *section,
                                   const char *name,
                                   const char *value,
                                   bool *result)
{
  if (section == NULL || name == NULL || value == NULL) {
    return -1;
  }

  if (strcmp(section, section_simulator ) != 0
      || strcmp(name, name_enabled) != 0) {
    return -1;
  }

  if (strcmp(value, bool_names[(u8) true]) == 0) {
    *result = true;
  } else if (strcmp(value, bool_names[(u8) false]) == 0) {
    *result = false;
  } else {
    return -1;
  }

  return 0;
}

static bool simulator_enabled = false;
void sbp_read_resp_callback(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  (void)context;

  const char *section, *name, *value;
  if (parse_setting_read_resp(msg_, len, &section, &name, &value) == 0) {
    if(check_simulator_enabled(section, name, value, &simulator_enabled) == 0) {
      piksi_log(LOG_DEBUG, "Parsed simulator enable: %d", (u8)simulator_enabled);
    }
  }
}

void sbp_base_obs_invalid(double timediff)
{
  if (simulator_enabled) {
    piksi_log(LOG_DEBUG, "Skipping obs invalid");
    return;
  }

  piksi_log(LOG_WARNING, "received indication that base obs. are invalid, time difference: %f", timediff);

  static const char ntrip_sanity_failed[] = "<<BASE_OBS_SANITY_FAILED>>";
  static const size_t command_len = sizeof(ntrip_sanity_failed) - sizeof(ntrip_sanity_failed[0]);

  u8 msg_buf[sizeof(msg_command_req_t) + command_len];
  int msg_len = sizeof(msg_buf);

  msg_command_req_t* sbp_command = (msg_command_req_t*)msg_buf;
  memcpy(sbp_command->command, ntrip_sanity_failed, command_len);

  sbp_message_send(SBP_MSG_COMMAND_REQ, (u8)msg_len, (u8*)sbp_command, 0);
}
