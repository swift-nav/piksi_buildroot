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

#include "framer.h"

typedef void (*framer_init_fn_t)(void *state);
typedef uint32_t (*framer_process_fn_t)(void *state,
                                        const uint8_t *data,
                                        uint32_t data_length,
                                        const uint8_t **frame,
                                        uint32_t *frame_length);

typedef struct {
  framer_init_fn_t init;
  framer_process_fn_t process;
} framer_interface_t;

static const framer_interface_t framer_interfaces[] = {
  [FRAMER_NONE] = {
    .init = framer_none_init,
    .process = framer_none_process
  },
  [FRAMER_SBP] = {
    .init = framer_sbp_init,
    .process = framer_sbp_process
  },
  [FRAMER_RTCM3] = {
    .init = framer_rtcm3_init,
    .process = framer_rtcm3_process
  }
};

void framer_state_init(framer_state_t *s, framer_t framer)
{
  s->framer = framer;
  framer_interfaces[s->framer].init(&s->impl_framer_state);
}

uint32_t framer_process(framer_state_t *s,
                        const uint8_t *data, uint32_t data_length,
                        const uint8_t **frame, uint32_t *frame_length)
{
  return framer_interfaces[s->framer].process(&s->impl_framer_state,
                                              data, data_length,
                                              frame, frame_length);
}
