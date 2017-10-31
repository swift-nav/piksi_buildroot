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

#include <czmq.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>

#include <libsbp/navigation.h>
#include <libsbp/sbp.h>
#include <libsbp/system.h>

#include "sbp.h"
#include "udp_socket.h"

#define PROGRAM_NAME "sample_daemon"

#define SBP_SUB_ENDPOINT    ">tcp://127.0.0.1:43030"  /* SBP External Out */
#define SBP_PUB_ENDPOINT    ">tcp://127.0.0.1:43031"  /* SBP External In */

static double offset = 0;
static bool enable_broadcast = false;
static int broadcast_port = 56666;
static char broadcast_hostname[256] = "255.255.255.255";

static udp_broadcast_context udp_context;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMain options");
  puts("\t--offset <offset in meters>");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_OFFSET = 1
  };

  const struct option long_opts[] = {
    {"offset", required_argument, 0, OPT_ID_OFFSET},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case OPT_ID_OFFSET: {
        offset = atof(optarg);
      }
      break;

      default: {
        puts("Invalid option");
        return -1;
      }
      break;
    }
  }

  return 0;
}

static void heartbeat_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;

  sbp_log(LOG_DEBUG, "Got piksi heartbeat...");

  if (enable_broadcast) {
    sbp_send_message(&udp_context.sbp_state, SBP_MSG_HEARTBEAT, sender_id, len, msg, udp_write_callback);
    udp_flush_buffer(&udp_context);
  }
}

static void pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  msg_pos_llh_t *pos = (msg_pos_llh_t*)msg;

  double adjusted = pos->height - offset;

  static time_t last_log_msg_time = 0;
  time_t now = time(NULL);

  if (last_log_msg_time == 0 || now - last_log_msg_time >= 1L) {

    sbp_log(LOG_DEBUG, "SBP_MSG_POS_LLH: lat = %f lon = %f height = %f adjusted height = %f",
              pos->lat, pos->lon, pos->height, adjusted);

    last_log_msg_time = time(NULL);
  }

  unsigned int mode = pos->flags & TIME_SOURCE_MASK;
  if (mode != 0) {
    pos->height = adjusted;
  }

  if (enable_broadcast) {
    sbp_send_message(&udp_context.sbp_state, SBP_MSG_POS_LLH, sender_id, len, msg, udp_write_callback);
    udp_flush_buffer(&udp_context);
  }
}

static int notify_settings_changed(void *context)
{
  (void)context;

  sbp_log(LOG_DEBUG, "Settings changed: enable_broadcast = %d, broadcast port = %d, offset = %04.04f", enable_broadcast, broadcast_port, offset);

  close_udp_broadcast_socket(&udp_context);

  if (enable_broadcast)
    open_udp_broadcast_socket(&udp_context, broadcast_hostname, broadcast_port);

  return 0;
}

int main(int argc, char *argv[])
{
  int status = EXIT_SUCCESS;
  settings_ctx_t *settings_ctx = NULL;
  sbp_zmq_pubsub_ctx_t *ctx = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    status = EXIT_FAILURE;
    goto cleanup;
  }

  sbp_log(LOG_INFO, "Launched, default offset: %02.04f", offset);

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  ctx = sbp_zmq_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_init(sbp_zmq_pubsub_rx_ctx_get(ctx),
               sbp_zmq_pubsub_tx_ctx_get(ctx)) != 0) {
    sbp_log(LOG_ERR, "Error initializing SBP!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_callback_register(SBP_MSG_HEARTBEAT, heartbeat_callback, NULL) != 0) {
    sbp_log(LOG_ERR, "Error setting MSG_HEARTBEAT callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_callback_register(SBP_MSG_POS_LLH, pos_llh_callback, NULL) != 0) {
    sbp_log(LOG_ERR, "Error setting MSG_POS_LLH callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  /* Set up settings */
  settings_ctx = settings_create();

  if (settings_ctx == NULL) {
    sbp_log(LOG_ERR, "Error registering for settings!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (settings_reader_add(settings_ctx,
                          sbp_zmq_pubsub_zloop_get(ctx)) != 0) {
    sbp_log(LOG_ERR, "Error registering for settings read!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  settings_register(settings_ctx, "sample_daemon", "enable_broadcast",
                    &enable_broadcast, sizeof(enable_broadcast),
                    SETTINGS_TYPE_BOOL,
                    notify_settings_changed, NULL);

  settings_register(settings_ctx, "sample_daemon", "offset",
                    &offset, sizeof(offset),
                    SETTINGS_TYPE_FLOAT,
                    notify_settings_changed, NULL);

  settings_register(settings_ctx, "sample_daemon", "broadcast_port",
                    &broadcast_port, sizeof(broadcast_port),
                    SETTINGS_TYPE_INT,
                    notify_settings_changed, NULL);

  settings_register(settings_ctx, "sample_daemon", "broadcast_hostname",
                    &broadcast_hostname, sizeof(broadcast_hostname),
                    SETTINGS_TYPE_STRING,
                    notify_settings_changed, NULL);

  sbp_log(LOG_INFO, "Ready!");
  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

cleanup:
  sbp_zmq_pubsub_destroy(&ctx);
  settings_destroy(&settings_ctx);
  logging_deinit();

  if (enable_broadcast)
    close_udp_broadcast_socket(&udp_context);

  return status;
}
