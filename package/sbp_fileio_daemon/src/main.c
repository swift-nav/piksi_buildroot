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

#include <sbp_zmq.h>

#include "sbp_fileio.h"

int main(void)
{
  /* Set up SBP ZMQ */
  sbp_zmq_config_t sbp_zmq_config = {
    .sbp_sender_id = SBP_SENDER_ID,
    .pub_endpoint = ">tcp://localhost:43021",
    .sub_endpoint = ">tcp://localhost:43020"
  };

  sbp_zmq_state_t sbp_zmq_state;
  if (sbp_zmq_init(&sbp_zmq_state, &sbp_zmq_config) != 0) {
    exit(EXIT_FAILURE);
  }

  sbp_fileio_setup(&sbp_zmq_state);

  sbp_zmq_loop(&sbp_zmq_state);

  sbp_zmq_deinit(&sbp_zmq_state);
  exit(EXIT_SUCCESS);
}
