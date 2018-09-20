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

#include <float.h>
#include <getopt.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings.h>

#include <libsbp/navigation.h>
#include <libsbp/sbp.h>
#include <libsbp/system.h>

#include "sbp.h"
#include "udp_socket.h"

#define PROGRAM_NAME "sample_daemon"

static double offset = 0;
static bool enable_broadcast = false;
static int broadcast_port = 56666;
static char broadcast_hostname[256] = "255.255.255.255";
static bool ntrip_enable = false;

static udp_broadcast_context udp_context;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMain options");
  puts("\t--offset <offset in meters>");
}

static int parse_options(int argc, char *argv[])
{
  enum { OPT_ID_OFFSET = 1 };

  const struct option long_opts[] = {
    {"offset", required_argument, 0, OPT_ID_OFFSET},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_OFFSET: {
      offset = atof(optarg);
    } break;

    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }

  return 0;
}

static void heartbeat_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;

  piksi_log(LOG_DEBUG | LOG_SBP, "Got piksi heartbeat...");

  if (enable_broadcast) {
    sbp_send_message(&udp_context.sbp_state,
                     SBP_MSG_HEARTBEAT,
                     sender_id,
                     len,
                     msg,
                     udp_write_callback);
    udp_flush_buffer(&udp_context);
  }
}

static void pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  msg_pos_llh_t *pos = (msg_pos_llh_t *)msg;

  double adjusted = pos->height - offset;

  static time_t last_log_msg_time = 0;
  time_t now = time(NULL);

  if (last_log_msg_time == 0 || now - last_log_msg_time >= 1L) {

    piksi_log(LOG_DEBUG | LOG_SBP,
              "SBP_MSG_POS_LLH: lat = %f lon = %f height = %f adjusted height = %f",
              pos->lat,
              pos->lon,
              pos->height,
              adjusted);

    last_log_msg_time = time(NULL);
  }

  unsigned int mode = pos->flags & TIME_SOURCE_MASK;
  if (mode != 0) {
    pos->height = adjusted;
  }

  if (enable_broadcast) {
    sbp_send_message(&udp_context.sbp_state,
                     SBP_MSG_POS_LLH,
                     sender_id,
                     len,
                     msg,
                     udp_write_callback);
    udp_flush_buffer(&udp_context);
  }
}

static int notify_settings_changed(void *context)
{
  (void)context;

  piksi_log(LOG_DEBUG | LOG_SBP,
            "Settings changed: enable_broadcast = %d, broadcast port = %d, offset = %04.04f",
            enable_broadcast,
            broadcast_port,
            offset);
  piksi_log(LOG_DEBUG | LOG_SBP, "Settings watched: ntrip_enable= %d", ntrip_enable);

  close_udp_broadcast_socket(&udp_context);

  if (enable_broadcast) open_udp_broadcast_socket(&udp_context, broadcast_hostname, broadcast_port);

  return 0;
}

int main(int argc, char *argv[])
{
  int status = EXIT_SUCCESS;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    status = EXIT_FAILURE;
    goto cleanup;
  }

  piksi_log(LOG_INFO | LOG_SBP, "Launched, default offset: %02.04f", offset);
  if (sbp_init() != 0) {
    piksi_log(LOG_ERR | LOG_SBP, "Error initializing SBP!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_callback_register(SBP_MSG_HEARTBEAT, heartbeat_callback, NULL) != 0) {
    piksi_log(LOG_ERR | LOG_SBP, "Error setting MSG_HEARTBEAT callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_callback_register(SBP_MSG_POS_LLH, pos_llh_callback, NULL) != 0) {
    piksi_log(LOG_ERR | LOG_SBP, "Error setting MSG_POS_LLH callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  /* Set up settings */
  settings_ctx_t *settings_ctx = sbp_get_settings_ctx();

  settings_register(settings_ctx,
                    "sample_daemon",
                    "enable_broadcast",
                    &enable_broadcast,
                    sizeof(enable_broadcast),
                    SETTINGS_TYPE_BOOL,
                    notify_settings_changed,
                    NULL);

  settings_register(settings_ctx,
                    "sample_daemon",
                    "offset",
                    &offset,
                    sizeof(offset),
                    SETTINGS_TYPE_FLOAT,
                    notify_settings_changed,
                    NULL);

  settings_register(settings_ctx,
                    "sample_daemon",
                    "broadcast_port",
                    &broadcast_port,
                    sizeof(broadcast_port),
                    SETTINGS_TYPE_INT,
                    notify_settings_changed,
                    NULL);

  settings_register(settings_ctx,
                    "sample_daemon",
                    "broadcast_hostname",
                    &broadcast_hostname,
                    sizeof(broadcast_hostname),
                    SETTINGS_TYPE_STRING,
                    notify_settings_changed,
                    NULL);

  settings_add_watch(settings_ctx,
                     "ntrip",
                     "enable",
                     &ntrip_enable,
                     sizeof(ntrip_enable),
                     SETTINGS_TYPE_BOOL,
                     notify_settings_changed,
                     NULL);

  piksi_log(LOG_INFO | LOG_SBP, "Ready!");
  sbp_run();

cleanup:
  sbp_deinit();
  logging_deinit();

  if (enable_broadcast) close_udp_broadcast_socket(&udp_context);

  return status;
}
