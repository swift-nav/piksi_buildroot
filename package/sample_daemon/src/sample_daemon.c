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

#include <curl/curl.h>
#include <czmq.h>
#include <float.h>
#include <getopt.h>
#include <libnetwork.h>
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/util.h>
#include <libpiksi/logging.h>
#include <libsbp/navigation.h>
#include <libsbp/system.h>
#include <libsbp/sbp.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sbp.h"

#define PROGRAM_NAME "sample_daemon"

#define SBP_SUB_ENDPOINT    ">tcp://127.0.0.1:43030"  /* SBP External Out */
#define SBP_PUB_ENDPOINT    ">tcp://127.0.0.1:43031" /* SBP External In */

static double offset = 0;

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
  (void) sender_id;
  (void) len;
  (void) msg;
  (void) context;

  sbp_log(LOG_DEBUG, "Got piksi heartbeat...");
}

static void pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) sender_id;
  (void) len;
  (void) context;
  msg_pos_llh_t *pos = (msg_pos_llh_t*)msg;

  double adjusted = pos->height - offset;

  printf("SBP_MSG_POS_LLH: lat = %f lon = %f height = %f adjusted height = %f\n", pos->lat, pos->lon, pos->height, adjusted);

  unsigned int mode = pos->flags & 0x07;
  if (mode != 0) {
    pos->height = adjusted;
  }

  sbp_message_send(SBP_MSG_POS_LLH, sizeof(*pos), (u8 *)pos);
}

int main(int argc, char *argv[])
{
  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  puts("Hello World!\n");

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(SBP_PUB_ENDPOINT,
                                                    SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  if (sbp_init(sbp_zmq_pubsub_rx_ctx_get(ctx),
               sbp_zmq_pubsub_tx_ctx_get(ctx)) != 0) {
    piksi_log(LOG_ERR, "Error initializing SBP!");
    exit(EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_HEARTBEAT, heartbeat_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "Error setting MSG_POS_LLH callback!");
    exit(EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_POS_LLH, pos_llh_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "Error setting MSG_POS_LLH callback!");
    exit(EXIT_FAILURE);
  }

  puts("Ready!\n");
  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

  sbp_zmq_pubsub_destroy(&ctx);

  exit(EXIT_SUCCESS);
}
