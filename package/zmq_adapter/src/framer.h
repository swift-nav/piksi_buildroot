/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_FRAMER_H
#define SWIFTNAV_FRAMER_H

#include <stdint.h>
#include <stdbool.h>

#include "framer_none.h"
#include "framer_sbp.h"
#include "framer_rtcm3.h"

typedef enum {
  FRAMER_NONE,
  FRAMER_SBP,
  FRAMER_RTCM3
} framer_t;

typedef struct {
  framer_t framer;
  union {
    framer_none_state_t framer_none_state;
    framer_sbp_state_t framer_sbp_state;
    framer_rtcm3_state_t framer_rtcm3_state;
  } impl_framer_state;
} framer_state_t;

void framer_state_init(framer_state_t *s, framer_t framer);
uint32_t framer_process(framer_state_t *s,
                        const uint8_t *data, uint32_t data_length,
                        const uint8_t **frame, uint32_t *frame_length);

#endif /* SWIFTNAV_FRAMER_H */
