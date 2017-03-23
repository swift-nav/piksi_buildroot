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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RTCM3_SUB_ENDPOINT ">tcp://127.0.0.1:45010"
#define SBP_PUB_ENDPOINT ">tcp://127.0.0.1:43031"

static int file_read_string(const char *filename, char *str, size_t str_size) {
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

int main(int argc, char *argv[]) {
  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  /* Read SBP sender ID */
  u16 sbp_sender_id = SBP_SENDER_ID;
  char sbp_sender_id_string[32];
  if (file_read_string("/cfg/sbp_sender_id", sbp_sender_id_string,
                       sizeof(sbp_sender_id_string)) == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }

  if (sbp_init(sbp_sender_id, SBP_PUB_ENDPOINT) != 0) {
    exit(EXIT_FAILURE);
  }

  zsock_t *rtcm3_sub = zsock_new_sub(RTCM3_SUB_ENDPOINT, "");
  if (rtcm3_sub == NULL) {
    printf("error creating SUB socket\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    zmsg_t *msg = zmsg_recv(rtcm3_sub);
    if (msg == NULL) {
      continue;
    }

    zframe_t *frame;
    for (frame = zmsg_first(msg); frame != NULL; frame = zmsg_next(msg)) {
      rtcm3_decode_frame(zframe_data(frame), zframe_size(frame));
    }

    zmsg_destroy(&msg);
  }

  exit(EXIT_SUCCESS);
}
