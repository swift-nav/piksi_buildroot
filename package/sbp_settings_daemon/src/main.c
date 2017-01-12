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

#include "settings.h"

static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("error opening %s\n", filename);
    return -1;
  }

  bool success = (fgets(str, str_size, fp) != NULL);

  fclose(fp);

  if (!success) {
    printf("error reading %s\n", filename);
    return -1;
  }

  return 0;
}

int main(void)
{
  /* Set up SBP ZMQ */
  u16 sbp_sender_id = SBP_SENDER_ID;
  char sbp_sender_id_string[32];
  if (file_read_string("/cfg/sbp_sender_id", sbp_sender_id_string,
                        sizeof(sbp_sender_id_string)) == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }
  sbp_zmq_config_t sbp_zmq_config = {
    .sbp_sender_id = sbp_sender_id,
    .pub_endpoint = ">tcp://localhost:43021",
    .sub_endpoint = ">tcp://localhost:43020"
  };

  sbp_zmq_state_t sbp_zmq_state;
  if (sbp_zmq_init(&sbp_zmq_state, &sbp_zmq_config) != 0) {
    exit(EXIT_FAILURE);
  }

  settings_setup(&sbp_zmq_state);

  sbp_zmq_loop(&sbp_zmq_state);

  sbp_zmq_deinit(&sbp_zmq_state);
  exit(EXIT_SUCCESS);
}

