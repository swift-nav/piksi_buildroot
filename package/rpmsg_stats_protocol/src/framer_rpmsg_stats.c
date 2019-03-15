/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libpiksi/logging.h>

#include <rpmsg_stats/rpmsg_stats.h>

typedef struct {
  union d {
    uint8_t buffer[sizeof(rpmsg_stats_t)];
    rpmsg_stats_t stats;
  }
  size_t offset;
  bool need_signature;
} framer_rpmsg_state_t;

void *framer_create(void)
{
  framer_rpmsg_state_t *s = malloc(sizeof(*s));
  s->need_signature = true;
  s->offset = 0;
  if (s == NULL) return NULL;
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
                        const uint8_t **frame_out,
                        uint32_t *frame_length_out)
{
  framer_rpmsg_state_t *s = (framer_rpmsg_state_t *)state;

  if (data_length == 0) return 0;

  uint32_t offset = 0;
  uint32_t frame_length = 0;

  while (offset < data_length) {
    if (s->need_signature) {
      if (sizeof(s->d.stats.signature) <= (data_length - offset)) {
        uint32_t signature = RPMSG_STATS_SIGNATURE;
        if (memcmp(data[offset], &signature, sizeof(signature)) == 0) {
          s->need_signature = false;
        } 
        offset += 4;
      } else {
        offset = data_length;
      }
    } else {
      s->d.buffer[s->offset++] = data[offset++];
    }
  }
}
