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

#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/loop.h>
#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/sbp_rx.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>

#include <libsbp/sbp.h>
#include <libsbp/system.h>
#include <libsbp/logging.h>

#define PROGRAM_NAME "pfw_welcome"

#define SBP_SUB_ENDPOINT    "ipc:///var/run/sockets/external.pub"  /* SBP External Out */
#define SBP_PUB_ENDPOINT    "ipc:///var/run/sockets/external.sub"  /* SBP External In */

static pk_loop_t *loop = NULL;

static void heartbeat_callback(u16 sender_id, u8 len, u8 msg[], void *context) {
  (void) sender_id;
  (void) len;
  (void) context;

  msg_heartbeat_t *msg_hb = (msg_heartbeat_t*) msg;

  if (msg_hb->flags & 0x7) {
    sbp_log(LOG_ERR, "firmware claiming an error state: 0x%08x", msg_hb->flags);
  }

  pk_loop_stop(loop);
}

int main(int argc, char *argv[]) {
  (void) argc;
  (void) argv;

  int status = EXIT_SUCCESS;
  sbp_pubsub_ctx_t *ctx = NULL;

  logging_init(PROGRAM_NAME);

  sbp_log(LOG_INFO, PROGRAM_NAME " launched");

  loop = pk_loop_create();
  if (loop == NULL) {
    status = EXIT_FAILURE;
    goto cleanup;
  }

  ctx = sbp_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    status = EXIT_FAILURE;
    goto cleanup;
  }

  sbp_rx_ctx_t *rx_ctx = sbp_pubsub_rx_ctx_get(ctx);

  if ((NULL == rx_ctx)) {
    sbp_log(LOG_ERR, "Error initializing SBP!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_rx_attach(rx_ctx, loop) != 0) {
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_rx_callback_register(rx_ctx, SBP_MSG_HEARTBEAT, heartbeat_callback, ctx, NULL) != 0) {
    sbp_log(LOG_ERR, "Error setting SBP_MSG_HEARTBEAT callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  pk_loop_run_simple(loop);

cleanup:
  sbp_pubsub_destroy(&ctx);
  pk_loop_destroy(&loop);
  logging_deinit();

  return status;
}
