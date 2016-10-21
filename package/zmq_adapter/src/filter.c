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

#include "filter.h"

typedef void (*filter_init_fn_t)(void *state,
                                 const char *filename);
typedef int (*filter_process_fn_t)(void *state,
                                   const uint8_t *msg,
                                   uint32_t msg_length);

typedef struct {
  filter_init_fn_t init;
  filter_process_fn_t process;
} filter_interface_t;

static const filter_interface_t filter_interfaces[] = {
  [FILTER_NONE] = {
    .init = filter_none_init,
    .process = filter_none_process
  },
  [FILTER_SBP] = {
    .init = filter_sbp_init,
    .process = filter_sbp_process
  }
};

void filter_state_init(filter_state_t *s, filter_t filter,
                       const char *filename)
{
  s->filter = filter;
  filter_interfaces[s->filter].init(&s->impl_filter_state, filename);
}

int filter_process(filter_state_t *s,
                   const uint8_t *msg, uint32_t msg_length)
{
  return filter_interfaces[s->filter].process(&s->impl_filter_state,
                                              msg, msg_length);
}
