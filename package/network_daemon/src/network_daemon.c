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

#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <czmq.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/settings.h>
#include <libpiksi/networking.h>
#include <libpiksi/util.h>

#include <libsbp/sbp.h>
#include <libsbp/piksi.h>
#include <libsbp/system.h>

#define PROGRAM_NAME "network_daemon"

#define SBP_SUB_ENDPOINT    ">tcp://127.0.0.1:43060"  /* SBP Internal Out */
#define SBP_PUB_ENDPOINT    ">tcp://127.0.0.1:43061"  /* SBP Internal In */

#define SBP_FRAMING_MAX_PAYLOAD_SIZE (255u)
#define SBP_MAX_NETWORK_INTERFACES (10u)
#define NETWORK_HEARTBEAT_SEEN_TO_UPDATE (30u)
#define NETWORK_USAGE_UPDATE_INTERVAL (NETWORK_HEARTBEAT_SEEN_TO_UPDATE * 1000u)

u8 *interface = NULL;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMain options");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_INTERFACE = 1
  };

  const struct option long_opts[] = {
    {"interface", required_argument, 0, OPT_ID_INTERFACE},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case OPT_ID_INTERFACE: {
        interface = (u8*)optarg;
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

/**
 * @brief send_network_usage_update
 *
 * This will query the underlying network interface APIs
 * and generate a usage message based on the interfaces found.
 * @param pubsub_ctx: sbp zmq pubsub context used to send sbp message
 */
static void send_network_usage_update(sbp_zmq_pubsub_ctx_t *pubsub_ctx)
{
  network_usage_t usage_entries[SBP_MAX_NETWORK_INTERFACES];
  memset(usage_entries, 0, sizeof(usage_entries));

  u8 total_interfaces = 0;
  query_network_usage(usage_entries, SBP_MAX_NETWORK_INTERFACES, &total_interfaces);

  if (total_interfaces > 0) {
    msg_network_bandwidth_usage_t *bandwidth_msg = (msg_network_bandwidth_usage_t *)usage_entries;
    size_t message_length = sizeof(network_usage_t) * total_interfaces;
    if (message_length > SBP_FRAMING_MAX_PAYLOAD_SIZE ) {
      piksi_log(LOG_ERR, "Network usage structs surpassing SBP frame size");
      return;
    } else {
      sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(pubsub_ctx),
                      SBP_MSG_NETWORK_BANDWIDTH_USAGE,
                      (u8)(0xFF & message_length),
                      (u8*)bandwidth_msg);
    }
  }
}

/**
 * @brief usage_timer_callback - used to trigger usage updates
 */
static int usage_timer_callback(zloop_t *loop, int timer_id, void *arg)
{
  (void)loop;
  (void)timer_id;
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = (sbp_zmq_pubsub_ctx_t *)arg;
  assert(pubsub_ctx != NULL);

  send_network_usage_update(pubsub_ctx);

  return 0;
}

static int cleanup(sbp_zmq_pubsub_ctx_t **pubsub_ctx_loc,
                   int status);

int main(int argc, char *argv[])
{
  sbp_zmq_pubsub_ctx_t *ctx = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    return cleanup(&ctx, EXIT_FAILURE);
  }

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  ctx = sbp_zmq_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    return cleanup(&ctx, EXIT_FAILURE);
  }

  zloop_t *loop = sbp_zmq_pubsub_zloop_get(ctx);
  if (loop == NULL) {
    return cleanup(&ctx, EXIT_FAILURE);
  }
  if (zloop_timer(loop, NETWORK_USAGE_UPDATE_INTERVAL, 0, usage_timer_callback, ctx) == -1) {
    return cleanup(&ctx, EXIT_FAILURE);
  }

  zmq_simple_loop(loop);

  return cleanup(&ctx, EXIT_SUCCESS);
}

static int cleanup(sbp_zmq_pubsub_ctx_t **pubsub_ctx_loc,
                   int status) {
  sbp_zmq_pubsub_destroy(pubsub_ctx_loc);
  logging_deinit();

  return status;
}
