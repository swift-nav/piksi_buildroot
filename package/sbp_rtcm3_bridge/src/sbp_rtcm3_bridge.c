/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "sbp.h"
#include "rtcm3_decode.h"
#include "sbp_rtcm3.h"
#include <assert.h>
#include <czmq.h>
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGRAM_NAME "sbp_rtcm3_bridge"

#define RTCM3_SUB_ENDPOINT  ">tcp://127.0.0.1:45010"  /* RTCM3 External */
#define SBP_SUB_ENDPOINT    ">tcp://127.0.0.1:43010"  /* SBP Firmware */
#define SBP_PUB_ENDPOINT    ">tcp://127.0.0.1:43031"  /* SBP External */

static int rtcm3_reader_handler(zloop_t *zloop, zsock_t *zsock, void *arg)
{
  zmsg_t *msg;
  while (1) {
    msg = zmsg_recv(zsock);
    if (msg != NULL) {
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in zmsg_recv()");
      return -1;
    }
  }

  zframe_t *frame;
  for (frame = zmsg_first(msg); frame != NULL; frame = zmsg_next(msg)) {
    rtcm3_decode_frame(zframe_data(frame), zframe_size(frame));
  }

  zmsg_destroy(&msg);
  return 0;
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(SBP_PUB_ENDPOINT,
                                                    SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  zsock_t *rtcm3_sub = zsock_new_sub(RTCM3_SUB_ENDPOINT, "");
  if (rtcm3_sub == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(EXIT_FAILURE);
  }

  if (zloop_reader(sbp_zmq_pubsub_zloop_get(ctx), rtcm3_sub,
                   rtcm3_reader_handler, NULL) != 0) {
    piksi_log(LOG_ERR, "error adding reader");
    exit(EXIT_FAILURE);
  }

  /* TODO: register SBP callbacks */

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

  exit(EXIT_SUCCESS);
}
