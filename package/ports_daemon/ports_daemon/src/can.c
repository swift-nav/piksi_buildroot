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
#include "ports.h"

/* can_settings_initialized prevents false warning when notify function
 * is triggered by read from persistent config file during boot */
static bool can_settings_initialized = false;

static const char *const bitrate_enum_names[] =
  {"10k", "20k", "50k", "125k", "250k", "500k", "1M", NULL};
enum {
  BITRATE_10K,
  BITRATE_20K,
  BITRATE_50K,
  BITRATE_125K,
  BITRATE_250K,
  BITRATE_500K,
  BITRATE_1M,
};

static const u32 bitrate_val_table[] = {10000, 20000, 50000, 125000, 250000, 500000, 1000000};

typedef struct {
  char *name;
  u32 id;
  u32 filter;
  u8 bitrate;
} can_t;

static can_t cans[2] = {[0] = {.name = "can0", .id = 0, .filter = 0, .bitrate = BITRATE_250K},
                        [1] = {.name = "can1", .id = 1, .filter = 0, .bitrate = BITRATE_250K}};

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

  snprintf_assert(cmd, sizeof(cmd), "sudo ip link set %s type can bitrate %d", name, br);

  return can_cmd(cmd);
}

static int can_set_txqueuelen_piksi(const char *name, int txqlen)
{
  char cmd[100];

  snprintf_assert(cmd, sizeof(cmd), "sudo ip link set %s txqueuelen %d", name, txqlen);

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
  if (can_set_bitrate_piksi(can->name, bitrate_val_table[can->bitrate])) {
    piksi_log(LOG_ERR,
              "Could not set bitrate %" PRId32 " on interface %s",
              bitrate_val_table[can->bitrate],
              can->name);
    return 1;
  }

  if (can_set_txqueuelen_piksi(can->name, 1000)) {
    piksi_log(LOG_ERR, "Could not set tx queue len %" PRId32 " on interface %s", 1000, can->name);
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

static int can_notify(void *context)
{
  const can_t *can = (can_t *)context;

  if (can_settings_initialized && port_is_enabled(can->name)) {
    sbp_log(LOG_WARNING, "%s must be disabled to modify settings", can->name);
    return 1;
  }

  return 0;
}

static int bitrate_notify(void *context)
{
  const can_t *can = (can_t *)context;

  if (can_settings_initialized && port_is_enabled(can->name)) {
    sbp_log(LOG_WARNING, "%s must be disabled to modify settings", can->name);
    return 1;
  }

  return can_configure(can);
}

int can_init(pk_settings_ctx_t *settings_ctx)
{
  can_configure(&cans[0]);
  if (!device_is_duro()) {
    can_configure(&cans[1]);
  }

  /* Register settings */
  settings_type_t settings_type_bitrate;
  pk_settings_register_enum(settings_ctx, bitrate_enum_names, &settings_type_bitrate);

  pk_settings_register(settings_ctx,
                       "can0",
                       "bitrate",
                       &cans[0].bitrate,
                       sizeof(cans[0].bitrate),
                       settings_type_bitrate,
                       bitrate_notify,
                       &cans[0]);

  pk_settings_register(settings_ctx,
                       "can0",
                       "tx_id",
                       &cans[0].id,
                       sizeof(cans[0].id),
                       SETTINGS_TYPE_INT,
                       can_notify,
                       &cans[0]);

  pk_settings_register(settings_ctx,
                       "can0",
                       "rx_id_filter",
                       &cans[0].filter,
                       sizeof(cans[0].filter),
                       SETTINGS_TYPE_INT,
                       can_notify,
                       &cans[0]);

  if (!device_is_duro()) {
    pk_settings_register(settings_ctx,
                         "can1",
                         "bitrate",
                         &cans[1].bitrate,
                         sizeof(cans[1].bitrate),
                         settings_type_bitrate,
                         bitrate_notify,
                         &cans[1]);

    pk_settings_register(settings_ctx,
                         "can1",
                         "tx_id",
                         &cans[1].id,
                         sizeof(cans[1].id),
                         SETTINGS_TYPE_INT,
                         can_notify,
                         &cans[1]);

    pk_settings_register(settings_ctx,
                         "can1",
                         "rx_id_filter",
                         &cans[1].filter,
                         sizeof(cans[1].filter),
                         SETTINGS_TYPE_INT,
                         can_notify,
                         &cans[1]);
  }

  can_settings_initialized = true;

  return 0;
}

u32 can_get_id(u8 can_number)
{
  assert(can_number < sizeof(cans) / sizeof(cans[0]));
  return cans[can_number].id;
}

u32 can_get_filter(u8 can_number)
{
  assert(can_number < sizeof(cans) / sizeof(cans[0]));
  return cans[can_number].filter;
}
