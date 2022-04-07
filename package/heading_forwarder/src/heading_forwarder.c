/*
 * Copyright (C) 2019 Swift Navigation Inc.
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
#include <libpiksi/loop.h>

#include <libsbp/orientation.h>
#include <libsbp/navigation.h>

#define PROGRAM_NAME "heading_forwarder"

#define SBP_SUB_ENDPOINT "ipc:///var/run/sockets/heading_forward.pub" /* SBP Heading Out */
#define SBP_PUB_ENDPOINT "ipc:///var/run/sockets/heading_forward.sub" /* SBP Heading In */

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMain options");
}

static int parse_options(int argc, char *argv[])
{
  // clang-format off
  const struct option long_opts[] = {
    { 0, 0, 0, 0 },
  };
  // clang-format on

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {

    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }

  return 0;
}

/**
 * @brief baseline_heading_callback - forward baseline heading messages with
 */
static void baseline_heading_callback(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  sbp_pubsub_ctx_t *pubsub_ctx = (sbp_pubsub_ctx_t *)context;

  if (sbp_tx_send_from(sbp_pubsub_tx_ctx_get(pubsub_ctx),
                       SBP_MSG_BASELINE_HEADING,
                       len,
                       msg_,
                       sender_id)
      < 0) {
    PK_LOG_ANNO(LOG_ERR, "Failed to forward baseline heading from sender_id: %d", sender_id);
  }
}

/**
 * @brief baseline_ned_callback - forward baseline ned messages with
 */
static void baseline_ned_callback(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  sbp_pubsub_ctx_t *pubsub_ctx = (sbp_pubsub_ctx_t *)context;

  if (sbp_tx_send_from(sbp_pubsub_tx_ctx_get(pubsub_ctx),
                       SBP_MSG_BASELINE_NED,
                       len,
                       msg_,
                       sender_id)
      < 0) {
    PK_LOG_ANNO(LOG_ERR, "Failed to forward baseline ned from sender_id: %d", sender_id);
  }
}


static int cleanup(pk_loop_t **pk_loop_loc, sbp_pubsub_ctx_t **pubsub_ctx_loc, int status);

int main(int argc, char *argv[])
{
  pk_loop_t *loop = NULL;
  sbp_pubsub_ctx_t *pubsub_ctx = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(cleanup(&loop, &pubsub_ctx, EXIT_FAILURE));
  }

  loop = pk_loop_create();
  if (loop == NULL) {
    exit(cleanup(&loop, &pubsub_ctx, EXIT_FAILURE));
  }

  pubsub_ctx = sbp_pubsub_create(PROGRAM_NAME, SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (pubsub_ctx == NULL) {
    exit(cleanup(&loop, &pubsub_ctx, EXIT_FAILURE));
  }

  if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(pubsub_ctx), loop) != 0) {
    exit(cleanup(&loop, &pubsub_ctx, EXIT_FAILURE));
  }

  sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(pubsub_ctx),
                           SBP_MSG_BASELINE_HEADING,
                           baseline_heading_callback,
                           pubsub_ctx,
                           NULL);

  sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(pubsub_ctx),
                           SBP_MSG_BASELINE_NED,
                           baseline_ned_callback,
                           pubsub_ctx,
                           NULL);

  pk_loop_run_simple(loop);

  exit(cleanup(&loop, &pubsub_ctx, EXIT_SUCCESS));
}

static int cleanup(pk_loop_t **pk_loop_loc, sbp_pubsub_ctx_t **pubsub_ctx_loc, int status)
{
  pk_loop_destroy(pk_loop_loc);
  if (*pubsub_ctx_loc != NULL) {
    sbp_pubsub_destroy(pubsub_ctx_loc);
  }
  logging_deinit();

  return status;
}
