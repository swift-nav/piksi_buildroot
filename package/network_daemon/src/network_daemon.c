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

#include <libpiksi/logging.h>
#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/settings.h>
#include <libpiksi/networking.h>

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
 * @param pubsub_ctx: sbp pubsub context used to send sbp message
 */
static void send_network_usage_update(sbp_pubsub_ctx_t *pubsub_ctx)
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
      sbp_tx_send(sbp_pubsub_tx_ctx_get(pubsub_ctx),
                      SBP_MSG_NETWORK_BANDWIDTH_USAGE,
                      (u8)(0xFF & message_length),
                      (u8*)bandwidth_msg);
    }
  }
}

/**
 * @brief usage_timer_callback - used to trigger usage updates
 */
static void usage_timer_callback(pk_loop_t *loop, void *timer_handle, void *context)
{
  (void)loop;
  (void)timer_handle;
  sbp_pubsub_ctx_t *pubsub_ctx = (sbp_pubsub_ctx_t *)context;
  assert(pubsub_ctx != NULL);

  send_network_usage_update(pubsub_ctx);
}

static void signal_handler(pk_loop_t *pk_loop, void *handle, void *context)
{
  (void)context;
  int signal_value = pk_loop_get_signal_from_handle(handle);

  piksi_log(LOG_DEBUG, "Caught signal: %d", signal_value);

  pk_loop_stop(pk_loop);
}

static int cleanup(pk_loop_t **pk_loop_loc,
                   sbp_pubsub_ctx_t **pubsub_ctx_loc,
                   int status);

int main(int argc, char *argv[])
{
  pk_loop_t *loop = NULL;
  sbp_pubsub_ctx_t *ctx = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    return cleanup(&loop, &ctx, EXIT_FAILURE);
  }

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  loop = pk_loop_create();
  if (loop == NULL) {
    return cleanup(&loop, &ctx, EXIT_FAILURE);
  }

  if (pk_loop_signal_handler_add(loop, SIGINT, signal_handler, NULL) == NULL) {
    piksi_log(LOG_ERR, "Failed to add SIGINT handler to loop");
  }

  ctx = sbp_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    return cleanup(&loop, &ctx, EXIT_FAILURE);
  }

  if (pk_loop_timer_add(loop, NETWORK_USAGE_UPDATE_INTERVAL, usage_timer_callback, ctx) == NULL) {
    return cleanup(&loop, &ctx, EXIT_FAILURE);
  }

  pk_loop_run_simple(loop);
  piksi_log(LOG_DEBUG, "Network Daemon: Normal Exit");

  return cleanup(&loop, &ctx, EXIT_SUCCESS);
}

static int cleanup(pk_loop_t **pk_loop_loc,
                   sbp_pubsub_ctx_t **pubsub_ctx_loc,
                   int status) {
  pk_loop_destroy(pk_loop_loc);
  sbp_pubsub_destroy(pubsub_ctx_loc);
  logging_deinit();

  return status;
}
