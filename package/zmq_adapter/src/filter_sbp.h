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

#ifndef SWIFTNAV_FILTER_SBP_H
#define SWIFTNAV_FILTER_SBP_H

#include <stdint.h>
#include <stdbool.h>

#include <libsbp/sbp.h>

typedef struct {
  uint16_t type;
  uint8_t divisor;
  uint8_t counter;
} filter_sbp_rule_t;

typedef struct {
  filter_sbp_rule_t *rules;
  uint32_t rules_count;
  const char *config_file;
  int config_inotify;
} filter_sbp_state_t;

void filter_sbp_init(void *filter_sbp_state, const char *filename);
int filter_sbp_process(void *filter_sbp_state,
                       const uint8_t *msg, uint32_t msg_length);

#endif /* SWIFTNAV_FILTER_SBP_H */
