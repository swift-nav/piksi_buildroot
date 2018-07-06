/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/sbp_rx.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "firmware_state.h"
#include "manage_led.h"

#define PROGRAM_NAME "piksi_leds"

#define PUB_ENDPOINT_EXTERNAL_SBP "ipc:///var/run/sockets/external.sub"
#define SUB_ENDPOINT_EXTERNAL_SBP "ipc:///var/run/sockets/external.pub"

int main(void)
{
  int ret = EXIT_FAILURE;
  logging_init(PROGRAM_NAME);

  sbp_rx_ctx_t *ctx = sbp_rx_create(SUB_ENDPOINT_EXTERNAL_SBP);
  if (ctx == NULL) {
    goto cleanup;
  }

  pk_loop_t *loop = pk_loop_create();
  if (loop == NULL) {
    goto cleanup;
  }

  if (sbp_rx_attach(ctx, loop) != 0) {
    goto cleanup;
  }

  firmware_state_init(ctx);
  manage_led_setup(device_is_duro());

  pk_loop_run_simple(loop);

  ret = EXIT_SUCCESS;

cleanup:
  sbp_rx_destroy(&ctx);
  pk_loop_destroy(&loop);
  logging_deinit();
  exit(ret);
}
