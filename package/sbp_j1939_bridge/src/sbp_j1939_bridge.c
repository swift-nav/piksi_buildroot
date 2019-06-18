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

#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings_client.h>

#include <libsbp/sbp.h>
#include <libsbp/imu.h>

#include <libsettings/settings.h>

#include "sbp.h"

#define PROGRAM_NAME "sbp_j1939_bridge"

#define J1939_SUB_ENDPOINT "ipc:///var/run/sockets/j1939_internal.pub" /* J1939 Internal Out */
#define J1939_SUB_METRICS "j1939/sub"

#define J1939_PUB_ENDPOINT "ipc:///var/run/sockets/j1939_internal.sub" /* J1939 Internal In */
#define J1939_PUB_METRICS "j1939/pub"

#define J1939_HEADER_LENGTH 4

bool j1939_debug = false;

struct imu_raw_msg {
  u32 tow;
  u8 tow_f;
  u16 acc_x;
  u16 acc_y;
  u16 acc_z;
  u16 gyr_x;
  u16 gyr_y;
  u16 gyr_z;
};

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
  struct imu_raw_msg imu_raw;
};

struct j1939_message {
  int (*handler)(const u8 *payload, struct j1939_sbp_state *state);
  u32 PGN;
  u8 payload_length;
};

static int handle_slope_sensor_info_2(const u8 *payload, struct j1939_sbp_state *state)
{
  u32 pitch = 0;
  for (int i = 0; i < 3; i++) {
    pitch += payload[i] << (8 * i);
  }

  u32 roll = 0;
  for (int i = 0; i < 3; i++) {
    roll += payload[i + 3] << (8 * i);
  }

  (void)state;

  return 0;
}

static int handle_angular_rate(const u8 *payload, struct j1939_sbp_state *state)
{
  u16 angular_velocity[3] = {0, 0, 0}; /* x, y, z axes */
  for (int i = 0; i < 3; i++) {
    angular_velocity[i] = payload[i * 2] + (payload[i * 2 + 1] << 8);
  }

  state->imu_raw.gyr_x = angular_velocity[0];
  state->imu_raw.gyr_y = angular_velocity[1];
  state->imu_raw.gyr_z = angular_velocity[2];

  return 0;
}

static int handle_acceleration_sensor(const u8 *payload, struct j1939_sbp_state *state)
{
  u16 acceleration[3] = {0, 0, 0}; /* x, y, z axes */
  for (int i = 0; i < 3; i++) {
    acceleration[i] = payload[i * 2] + (payload[i * 2 + 1] << 8);
  }

  state->imu_raw.acc_x = acceleration[0];
  state->imu_raw.acc_y = acceleration[1];
  state->imu_raw.acc_z = acceleration[2];
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

static void parse_PGN(const u8 *data, struct j1939_sbp_state *state)
{
  u8 byte0 = data[3] & 0x1F; /* CAN ID is only 29 bit, not 32. little endian */
  state->priority = byte0 >> 2;
  state->extended_data_page = (byte0 >> 1) & 1;
  state->data_page = byte0 & 1;

  state->pdu_format = data[2];
  state->pdu_specific = data[1];
  state->source_address = data[0];

  state->PGN = state->extended_data_page << 17 | state->data_page << 16 | state->pdu_format << 8
               | state->pdu_specific;
}

static int j19392sbp_decode_frame(const u8 *data, const size_t length, void *context)
{
  struct j1939_sbp_state *state = (struct j1939_sbp_state *)context;

  if (length < J1939_HEADER_LENGTH) {
    piksi_log(LOG_WARNING, "j19392sbp_decode_frame too short");
    for (u8 i = 0; i < length; i++) {
      piksi_log(LOG_WARNING, "%02X ", data[i]);
    }
    return 0;
  } else {
    parse_PGN(data, state);
  }

  bool found = false;
  struct j1939_message *message;
  for (message = j1939_messages;
       message < j1939_messages + sizeof(j1939_messages) / sizeof(j1939_messages[0]);
       message++) {
    if (state->PGN == message->PGN) {
      found = true;
      if (message->payload_length != length - J1939_HEADER_LENGTH) {
        piksi_log(LOG_ERR,
                  "Wrong payload length for PGN: %d - Expected: %d Received %d",
                  message->PGN,
                  message->payload_length,
                  length - J1939_HEADER_LENGTH);
      } else {
        message->handler(data + J1939_HEADER_LENGTH, state);
      }
      break;
    }
  }

  if (!found) {
    piksi_log(LOG_WARNING, "Unknown PGN: %d", state->PGN);
    return 0;
  }

  switch (state->PGN) {
  case 61482: /* gyr_* info */
  case 61485: /* acc_* info */ break;

  default: break;
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
  pk_loop_t *loop = NULL;
  pk_endpoint_t *j1939_sub = NULL;

  /* High bit set means invalid time */
  j1939_to_sbp_state.imu_raw.tow = 1 << 31;
  j1939_to_sbp_state.imu_raw.tow_f = 0;

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

  j1939_pub = pk_endpoint_create(pk_endpoint_config()
                                   .endpoint(J1939_PUB_ENDPOINT)
                                   .identity(J1939_PUB_METRICS)
                                   .type(PK_ENDPOINT_PUB)
                                   .get());
  if (j1939_pub == NULL) {
    piksi_log(LOG_ERR, "error creating PUB socket");
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  j1939_sub = pk_endpoint_create(pk_endpoint_config()
                                   .endpoint(J1939_SUB_ENDPOINT)
                                   .identity(J1939_SUB_METRICS)
                                   .type(PK_ENDPOINT_SUB)
                                   .get());
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
