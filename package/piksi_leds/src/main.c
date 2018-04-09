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

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "firmware_state.h"
#include "manage_led.h"

#define PROGRAM_NAME "piksi_leds"

#define PUB_ENDPOINT_EXTERNAL_SBP ">tcp://localhost:43031"
#define SUB_ENDPOINT_EXTERNAL_SBP ">tcp://localhost:43030"

int main(void)
{
  logging_init(PROGRAM_NAME);

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(PUB_ENDPOINT_EXTERNAL_SBP,
                                                    SUB_ENDPOINT_EXTERNAL_SBP);
  if (ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  firmware_state_init(sbp_zmq_pubsub_rx_ctx_get(ctx));
  manage_led_setup(device_is_duro());

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

  sbp_zmq_pubsub_destroy(&ctx);
  exit(EXIT_SUCCESS);
}
