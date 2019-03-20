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

#include <libpiksi/logging.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define J1939_HEADER_LENGTH 4
#define J1939_DATA_LENGTH 8
#define J1939_FRAME_SIZE_MAX J1939_HEADER_LENGTH + J1939_DATA_LENGTH

typedef struct {
  uint8_t buffer[J1939_FRAME_SIZE_MAX];
} framer_j1939_state_t;

void *framer_create(void)
{
  framer_j1939_state_t *s = (framer_j1939_state_t *)malloc(sizeof(*s));
  if (s == NULL) {
    return NULL;
  }

  return (void *)s;
}

void framer_destroy(void **state)
{
  free(*state);
  *state = NULL;
}

uint32_t framer_process(void *state,
                        const uint8_t *data,
                        uint32_t data_length,
                        const uint8_t **frame,
                        uint32_t *frame_length)
{
  framer_j1939_state_t *s = (framer_j1939_state_t *)state;

  uint32_t data_offset = 0;
  *frame = NULL;
  *frame_length = 0;

  if (data_length >= 4) {
    memcpy(&s->buffer, data, data_length);
    *frame = s->buffer;
    *frame_length = data_length;
  } else {
    piksi_log(LOG_ERR, "J1939 frame too short, missing can_id - length: %d", data_length);
  }

  return data_length;
}
