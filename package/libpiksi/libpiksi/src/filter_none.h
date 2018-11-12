/*
 * Copyright (C) 2016-2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_FILTER_NONE_H
#define SWIFTNAV_FILTER_NONE_H

#include <stdint.h>
#include <stdbool.h>

void *filter_none_create(const char *filename);
void filter_none_destroy(void **state);
int filter_none_process(void *state, const uint8_t *msg, uint32_t msg_length);

#endif /* SWIFTNAV_FILTER_NONE_H */
