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

#ifndef SWIFTNAV_FRAMER_RTCM3_H
#define SWIFTNAV_FRAMER_RTCM3_H

#include <stdint.h>
#include <stdbool.h>

#define RTCM3_FRAME_SIZE_MAX (1029)

typedef struct {
  uint8_t buffer[RTCM3_FRAME_SIZE_MAX];
  uint32_t buffer_length;
  uint32_t refill_count;
  uint32_t remove_count;
} framer_rtcm3_state_t;

void framer_rtcm3_init(void *framer_rtcm3_state);
uint32_t framer_rtcm3_process(void *framer_rtcm3_state,
                              const uint8_t *data, uint32_t data_length,
                              const uint8_t **frame, uint32_t *frame_length);

#endif /* SWIFTNAV_FRAMER_RTCM3_H */
