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

#ifndef SWIFTNAV_FILTER_H
#define SWIFTNAV_FILTER_H

#include <stdint.h>
#include <stdbool.h>

#include "filter_none.h"
#include "filter_sbp.h"

typedef enum {
  FILTER_NONE,
  FILTER_SBP
} filter_t;

typedef struct {
  filter_t filter;
  union {
    filter_none_state_t filter_none_state;
    filter_sbp_state_t filter_sbp_state;
  } impl_filter_state;
} filter_state_t;

void filter_state_init(filter_state_t *s, filter_t filter,
                       const char *filename);
int filter_process(filter_state_t *s,
                   const uint8_t *msg, uint32_t msg_length);

#endif /* SWIFTNAV_FILTER_H */
