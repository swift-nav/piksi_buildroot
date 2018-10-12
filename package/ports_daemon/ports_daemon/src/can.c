/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#define _DEFAULT_SOURCE

#include <libsocketcan.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <linux/can.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "can.h"

static const char *const baudrate_enum_names[] = {"10000",
                                                  "20000",
                                                  "50000",
                                                  "125000",
                                                  "250000",
                                                  "500000",
                                                  "1000000",
                                                  NULL};
enum {
  BAUDRATE_10K,
  BAUDRATE_20K,
  BAUDRATE_50K,
  BAUDRATE_125K,
  BAUDRATE_250K,
  BAUDRATE_500K,
  BAUDRATE_1M,
};

static const u32 baudrate_val_table[] =
  {10000, 20000, 50000, 125000, 250000, 500000, 1000000};

typedef struct {
  char *name;
  u8 baudrate;
} can_t;

static can_t can0 = {
  .name = "can0",
  .baudrate = BAUDRATE_250K
};

static can_t can1 = {
  .name = "can1",
  .baudrate = BAUDRATE_250K
};

static int can_cmd(const char *cmd)
{
  int ret = WEXITSTATUS(system(cmd));

  if (ret) {
    piksi_log(LOG_ERR, "%s - ret: %d", cmd);
  }

  return ret;
}

static int can_set_bitrate_piksi(const char *name, int br)
{
  char cmd[100];

  snprintf_assert(cmd,
                  sizeof(cmd),
                  "sudo ip link set %s type can bitrate %d",
                  name,
                  br);

  return can_cmd(cmd);
}

static int can_do_start_piksi(const char *name)
{
  char cmd[100];

  snprintf_assert(cmd, sizeof(cmd), "sudo ifconfig %s up", name);

  return can_cmd(cmd);
}

static int can_do_stop_piksi(const char *name)
{
  char cmd[100];

  snprintf_assert(cmd, sizeof(cmd), "sudo ifconfig %s down", name);

  return can_cmd(cmd);
}

static int can_configure(const can_t *can)
{
  /* Get the current state of the CAN interfaces. */
  int state;
  if (can_get_state(can->name, &state)) {
    piksi_log(LOG_ERR, "Could not get %s state.", can->name);
    return 1;
  }

  piksi_log(LOG_ERR, "%s state %d", can->name, state);

  /* Bring the CAN interface down if they're up */
  if (state != CAN_STATE_STOPPED) {
    if (can_do_stop_piksi(can->name)) {
      piksi_log(LOG_ERR, "Could not stop %s.", can->name);
      return 1;
    }
  }

  /* Set the CAN bitrate. This must be done before turning interface back on. */
  if (can_set_bitrate_piksi(can->name, baudrate_val_table[can->baudrate])) {
    piksi_log(LOG_ERR,
              "Could not set bitrate %" PRId32 " on interface %s",
              baudrate_val_table[can->baudrate],
              can->name);
    return 1;
  }

  if (can_get_state(can->name, &state)) {
    piksi_log(LOG_ERR, "Could not get %s state.", can->name);
    return 1;
  }

  piksi_log(LOG_ERR, "%s state %d", can->name, state);

  /* Turn the CAN interface on. */
  if (can_do_start_piksi(can->name)) {
    piksi_log(LOG_ERR, "Could not start interface %s", can->name);
    return 1;
  }

  if (can_get_state(can->name, &state)) {
    piksi_log(LOG_ERR, "Could not get %s state.", can->name);
    return 1;
  }

  piksi_log(LOG_ERR, "%s state %d", can->name, state);

  return 0;
}

static int baudrate_notify(void *context)
{
  const can_t *can = (can_t *)context;
  return can_configure(can);
}

int can_init(settings_ctx_t *settings_ctx)
{
  can_configure(&can0);
  can_configure(&can1);

  /* Register settings */
  settings_type_t settings_type_baudrate;
  settings_type_register_enum(settings_ctx,
                              baudrate_enum_names,
                              &settings_type_baudrate);

  settings_register(settings_ctx,
                    "can0",
                    "baudrate",
                    &can0.baudrate,
                    sizeof(can0.baudrate),
                    settings_type_baudrate,
                    baudrate_notify,
                    &can0);

  settings_register(settings_ctx,
                    "can1",
                    "baudrate",
                    &can1.baudrate,
                    sizeof(can1.baudrate),
                    settings_type_baudrate,
                    baudrate_notify,
                    &can1);

  return 0;
}
