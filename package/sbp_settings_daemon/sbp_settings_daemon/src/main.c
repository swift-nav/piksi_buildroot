/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/logging.h>
#include "settings.h"

#define PROGRAM_NAME "sbp_settings_daemon"

#define PUB_ENDPOINT "tcp://localhost:43021"
#define SUB_ENDPOINT "tcp://localhost:43020"

int main(void)
{
  int ret = EXIT_FAILURE;
  logging_init(PROGRAM_NAME);

  sbp_pubsub_ctx_t *ctx = sbp_pubsub_create(PUB_ENDPOINT, SUB_ENDPOINT);
  if (ctx == NULL) {
    goto settings_cleanup;
  }

  pk_loop_t *loop = pk_loop_create();
  if (loop == NULL) {
    goto settings_cleanup;
  }

  if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(ctx), loop) != 0) {
    goto settings_cleanup;
  }

  settings_setup(sbp_pubsub_rx_ctx_get(ctx),
                 sbp_pubsub_tx_ctx_get(ctx));

  pk_loop_run_simple(loop);

  ret = EXIT_SUCCESS;

settings_cleanup:
  sbp_pubsub_destroy(&ctx);
  pk_loop_destroy(&loop);
  logging_deinit();
  exit(ret);
}
