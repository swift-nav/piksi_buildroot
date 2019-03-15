/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "sbp.h"
#include <assert.h>
#include <getopt.h>
#include <libpiksi/logging.h>
#include <libpiksi/settings.h>
#include <libsbp/sbp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGRAM_NAME "sbp_j1939_bridge"

#define J1939_SUB_ENDPOINT "ipc:///var/run/sockets/j1939_internal.pub" /* J1939 Internal Out */
#define J1939_PUB_ENDPOINT "ipc:///var/run/sockets/j1939_internal.sub" /* J1939 Internal In */

#define J1939_HEADER_LENGTH 4

bool j1939_debug = false;

struct j1939_sbp_state {
  u32 PGN;
  u8 priority;
  bool extended_data_page;
  bool data_page;
  u8 pdu_format;
  u8 pdu_specific;
  u8 source_address;
  u8 *payload;
  u8 payload_length;
};

struct j1939_message {
  int (*handler)(const u8 *payload, const u8 payload_length);
  u32 PGN;
  u8 payload_length;
};

static int handle_slope_sensor_info_2(const u8 *payload, const u8 payload_length)
{
  u32 pitch = 0;
  for (int i = 0; i < 3; i++) {
    pitch += payload[i] << (8 * i);
  }

  u32 roll = 0;
  for (int i = 0; i < 3; i++) {
    roll += payload[i + 3] << (8 * i);
  }

  (void) payload_length;
  piksi_log(LOG_ERR, "slope - pitch: %.2f roll %.2f", pitch / 32768.0 - 250, roll / 32768.0 - 250);
  return 0;
}

static int handle_angular_rate(const u8 *payload, const u8 payload_length)
{
  u16 angular_velocity[3] = {0, 0, 0}; /* x, y, z axes */
  for (int i = 0; i < 3; i++) {
    angular_velocity[i] = payload[i*2] + (payload[i*2 + 1] << 8);
  }

  (void) payload_length;
  piksi_log(LOG_ERR, "Angular Rate X, Y, Z : %.2f, %.2f, %.2f", angular_velocity[0] / 128.0 - 250, angular_velocity[1] / 128.0 - 250, angular_velocity[2] / 128.0 - 250);
  return 0;
}

static int handle_acceleration_sensor(const u8 *payload, const u8 payload_length)
{
  u16 acceleration[3] = {0, 0, 0}; /* x, y, z axes */
  for (int i = 0; i < 3; i++) {
    acceleration[i] = payload[i*2] + (payload[i*2 + 1] << 8);
  }

  (void) payload_length;
  piksi_log(LOG_ERR, "Acceleration X, Y, Z : %.2f, %.2f, %.2f", acceleration[0] - 320, acceleration[1] - 320, acceleration[2] - 320);
  return 0;
}

static struct j1939_message j1939_messages[] = {
  {
    .handler = handle_slope_sensor_info_2,
    .PGN = 61481,
    .payload_length = 8,
  },
  {
    .handler = handle_angular_rate,
    .PGN = 61482,
    .payload_length = 8,
  },
  {
    .handler = handle_acceleration_sensor,
    .PGN = 61485,
    .payload_length = 8,
  },
};

struct j1939_sbp_state j1939_to_sbp_state;

bool simulator_enabled_watch = false;

pk_endpoint_t *j1939_pub = NULL;

static int notify_simulator_enable_changed(void *context)
{
  (void)context;
  return 0;
}

static void parse_PGN(const u8 *data, struct j1939_sbp_state *state)
{
  u8 byte0 = data[3] & 0x1F; /* CAN ID is only 29 bit, not 32. little endian */
  state->priority = byte0 >> 2;
  state->extended_data_page = (byte0 & (1 << 1)) >> 1;
  state->data_page = byte0 & 1;

  state->pdu_format = data[2];
  state->pdu_specific = data[1];
  state->source_address = data[0];

  state->PGN =
    state->extended_data_page << 17 |
    state->data_page << 16 |
    state->pdu_format << 8 |
    state->pdu_specific;
}

static int j19392sbp_decode_frame(const u8 *data, const size_t length, void *context)
{
  (void)data;
  (void)length;
  (void)context;
  struct j1939_sbp_state *state = (struct j1939_sbp_state*) context;
  (void)state;

  if (length < 4) {
    piksi_log(LOG_ERR, "j19392sbp_decode_frame too short");
    for (u8 i = 0; i < length; i++) {
      piksi_log(LOG_ERR, "%02X ", data[i]);
    }
    return 0;
  } else {
    parse_PGN(data, state);
  }

  bool found = false;
  struct j1939_message *message;
  for (message = j1939_messages; message < j1939_messages + sizeof(j1939_messages) / sizeof(j1939_messages[0]); message++) {
    if (state->PGN == message->PGN) {
      message->handler(data + J1939_HEADER_LENGTH, length);
      found = true;
      break;
    }
  }

  if (!found) {
    piksi_log(LOG_ERR, "Unknown PGN: %d", state->PGN);
  }

  return 0;
}

static void j1939_reader_handler(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)handle;
  (void)status;

  pk_endpoint_t *j1939_sub_ept = (pk_endpoint_t *)context;
  if (pk_endpoint_receive(j1939_sub_ept, j19392sbp_decode_frame, &j1939_to_sbp_state) != 0) {
    piksi_log(LOG_ERR,
              "%s: error in %s (%s:%d): %s",
              __FUNCTION__,
              "pk_endpoint_receive",
              __FILE__,
              __LINE__,
              pk_endpoint_strerror());
  }
}

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMisc options");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_DEBUG = 1,
  };

  const struct option long_opts[] = {
    {"debug", no_argument, 0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_DEBUG: {
      j1939_debug = true;
    } break;

    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }
  return 0;
}

static int cleanup(pk_endpoint_t **j1939_ept_loc, int status);

int main(int argc, char *argv[])
{
  settings_ctx_t *settings_ctx = NULL;
  pk_loop_t *loop = NULL;
  pk_endpoint_t *j1939_sub = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  if (sbp_init() != 0) {
    piksi_log(LOG_ERR, "error initializing SBP");
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  j1939_sub = pk_endpoint_create(J1939_SUB_ENDPOINT, PK_ENDPOINT_SUB);
  if (j1939_sub == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  loop = sbp_get_loop();
  if (loop == NULL) {
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  if (pk_loop_endpoint_reader_add(loop, j1939_sub, j1939_reader_handler, j1939_sub) == NULL) {
    piksi_log(LOG_ERR, "error adding reader");
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  settings_ctx = sbp_get_settings_ctx();

  settings_add_watch(settings_ctx,
                     "simulator",
                     "enabled",
                     &simulator_enabled_watch,
                     sizeof(simulator_enabled_watch),
                     SETTINGS_TYPE_BOOL,
                     notify_simulator_enable_changed,
                     NULL);

  sbp_run();

  exit(cleanup(&j1939_sub, EXIT_SUCCESS));
}

static int cleanup(pk_endpoint_t **j1939_ept_loc, int status)
{
  pk_endpoint_destroy(j1939_ept_loc);
  sbp_deinit();
  logging_deinit();
  return status;
}
