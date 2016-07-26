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

#ifndef SWIFTNAV_FRAMER_SBP_H
#define SWIFTNAV_FRAMER_SBP_H

#include <stdint.h>
#include <stdbool.h>

#include <libsbp/sbp.h>

#define SBP_MSG_LEN_MAX (264)

typedef struct {
  sbp_state_t sbp_state;
  uint8_t send_buffer[SBP_MSG_LEN_MAX];
} framer_sbp_state_t;

void framer_sbp_init(void *framer_sbp_state);
uint32_t framer_sbp_process(void *framer_sbp_state,
                            const uint8_t *data, uint32_t data_length,
                            const uint8_t **frame, uint32_t *frame_length);

#endif /* SWIFTNAV_FRAMER_SBP_H */
