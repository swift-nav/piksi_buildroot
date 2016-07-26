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

#include "framer_sbp.h"

#include <string.h>
#include <stdio.h>

typedef struct {
  const uint8_t *read_buffer;
  uint32_t read_length;
  uint32_t read_offset;
  uint8_t *write_buffer;
  uint32_t write_length;
  uint32_t write_offset;
} sbp_io_context_t;

static u32 sbp_read(u8 *buff, u32 n, void* context)
{
  sbp_io_context_t *c = (sbp_io_context_t *)context;

  u32 count = c->read_length - c->read_offset;
  if (count > n) {
    count = n;
  }

  memcpy(buff, &c->read_buffer[c->read_offset], count);
  c->read_offset += count;
  return count;
}

static u32 sbp_write(u8 *buff, u32 n, void* context)
{
  sbp_io_context_t *c = (sbp_io_context_t *)context;

  u32 count = c->write_length - c->write_offset;
  if (count > n) {
    count = n;
  }

  memcpy(&c->write_buffer[c->write_offset], buff, count);
  c->write_offset += count;
  return count;
}

void framer_sbp_init(void *framer_sbp_state)
{
  framer_sbp_state_t *s = (framer_sbp_state_t *)framer_sbp_state;
  sbp_state_init(&s->sbp_state);
}

uint32_t framer_sbp_process(void *framer_sbp_state,
                            const uint8_t *data, uint32_t data_length,
                            const uint8_t **frame, uint32_t *frame_length)
{
  framer_sbp_state_t *s = (framer_sbp_state_t *)framer_sbp_state;

  sbp_io_context_t c;
  sbp_state_set_io_context(&s->sbp_state, &c);

  c.read_buffer = data;
  c.read_length = data_length;
  c.read_offset = 0;

  while (c.read_offset < data_length) {
    if (sbp_process(&s->sbp_state, sbp_read) == SBP_OK_CALLBACK_UNDEFINED) {

      /* A message was just decoded */
      c.write_buffer = s->send_buffer;
      c.write_length = sizeof(s->send_buffer);
      c.write_offset = 0;

      if (sbp_send_message(&s->sbp_state,
                           s->sbp_state.msg_type, s->sbp_state.sender_id,
                           s->sbp_state.msg_len, s->sbp_state.msg_buff,
                           sbp_write) == SBP_OK) {
        *frame = s->send_buffer,
        *frame_length = c.write_offset;
        return c.read_offset;
      } else {
        printf("SBP send error\n");
      }
    }
  }

  *frame = NULL;
  *frame_length = 0;
  return c.read_offset;
}
