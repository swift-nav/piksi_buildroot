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

typedef struct {
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
  for (int i = 0; i < data_length; i++) {
    piksi_log(LOG_ERR, "%02X", data[i]);
  }
  return data_length;
}
