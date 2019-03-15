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
  uint32_t buffer_length;
  uint32_t refill_count;
  uint32_t remove_count;
} framer_j1939_state_t;

void *framer_create(void)
{
  framer_j1939_state_t *s = (framer_j1939_state_t *)malloc(sizeof(*s));
  if (s == NULL) {
    return NULL;
  }

  s->buffer_length = 0;
  s->refill_count = 0;
  s->remove_count = 0;

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

  piksi_log(LOG_ERR, "J1939 framer_process");
  if (data_length >= 4) {
    piksi_log(LOG_ERR, "can_id: %02X%02X%02X%02X", data[3], data[2], data[1], data[0]);

  } else {
    piksi_log(LOG_ERR, "J1939 framer_process short");
    for (int i = 0; i < data_length; i++) {
      piksi_log(LOG_ERR, "%02X", data[i]);
    }
  }

  memcpy(&s->buffer, data, data_length);
  *frame = s->buffer;
  *frame_length = J1939_FRAME_SIZE_MAX;
  return data_length;
}
