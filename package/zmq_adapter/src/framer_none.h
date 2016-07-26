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

#ifndef SWIFTNAV_FRAMER_NONE_H
#define SWIFTNAV_FRAMER_NONE_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {

} framer_none_state_t;

void framer_none_init(void *framer_none_state);
uint32_t framer_none_process(void *framer_none_state,
                             const uint8_t *data, uint32_t data_length,
                             const uint8_t **frame, uint32_t *frame_length);

#endif /* SWIFTNAV_FRAMER_NONE_H */
