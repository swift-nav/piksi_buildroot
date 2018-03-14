/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#define PROTOBUF_FRAMER_BUFFER_SIZE_INCREMENT (255u)
#define PROTOBUF_FRAMER_BUFFER_SIZE_MAX (4096u)

#define PROTOBUF_FRAMER_PREAMBLE1 (0xFC)
#define PROTOBUF_FRAMER_PREAMBLE2 (0xCC)

enum PROTOBUF_FRAMER_STATE {
  PROTOBUF_FRAMER_STATE_IDLE = 0,
  PROTOBUF_FRAMER_STATE_PREAMBLE,
  PROTOBUF_FRAMER_STATE_LENGTH,
  PROTOBUF_FRAMER_STATE_COLLECT,
  PROTOBUF_FRAMER_STATE_FOUND,
  PROTOBUF_FRAMER_STATE_ERROR,
};

typedef struct {
  enum PROTOBUF_FRAMER_STATE state;
  uint8_t *frame_buffer;
  size_t frame_buffer_size;
  size_t count;
  size_t msg_length;
} framer_protobuf_state_t;

#if 1
#define STATIC_MEMPOOL_BUFFER 1
uint8_t memory_pool[PROTOBUF_FRAMER_BUFFER_SIZE_MAX];
#endif

static void protobuf_framer_buffer_dealloc(framer_protobuf_state_t *framer_state)
{
  if (framer_state != NULL && framer_state->frame_buffer != NULL) {
#ifndef STATIC_MEMPOOL_BUFFER
    free(framer_state->frame_buffer);
#endif
    memset(framer_state, 0, sizeof(*framer_state));
  }
}

static int8_t protobuf_framer_buffer_alloc(framer_protobuf_state_t *framer_state, size_t buffer_size)
{
  if (framer_state == NULL) {
    syslog(LOG_ERR, "Protobuf framing allocation passed null pointer");
    return -1;
  }

  if (buffer_size > PROTOBUF_FRAMER_BUFFER_SIZE_MAX) {
    syslog(LOG_ERR, "Protobuf framing allocation exceeded 'safe' limit");
    protobuf_framer_buffer_dealloc(framer_state);
    return -1;
  }

  buffer_size = buffer_size == 0 ?
                framer_state->frame_buffer_size + PROTOBUF_FRAMER_BUFFER_SIZE_INCREMENT
                : buffer_size;
  if (buffer_size < framer_state->frame_buffer_size) {
    return 0;
  }

#ifdef STATIC_MEMPOOL_BUFFER
  uint8_t *new_buffer = memory_pool;
#else
  uint8_t *new_buffer = (uint8_t *)malloc(buffer_size);
  if (new_buffer == NULL) {
    return -1;
  }
  memset(new_buffer, 0, sizeof(*framer_state));

  if (framer_state->frame_buffer != NULL) {
    if (framer_state->count > 0) {
      memcpy(new_buffer, framer_state->frame_buffer, framer_state->count);
    }
    free(framer_state->frame_buffer);
  }
#endif
  framer_state->frame_buffer = new_buffer;
  framer_state->frame_buffer_size = buffer_size;

  return 0;
}

static void protobuf_framer_reset(framer_protobuf_state_t *framer_state)
{
    protobuf_framer_buffer_dealloc(framer_state);
    protobuf_framer_buffer_alloc(framer_state, 0);
}

void * framer_create(void)
{
  framer_protobuf_state_t *new_framer_state = NULL;
  new_framer_state = (framer_protobuf_state_t *)malloc(sizeof(*new_framer_state));
  if (new_framer_state != NULL) {
    memset(new_framer_state, 0, sizeof(*new_framer_state));
  }
  return (void *)new_framer_state;
}

void framer_destroy(void **state)
{
  framer_protobuf_state_t **framer_loc = (framer_protobuf_state_t **)state;
  framer_protobuf_state_t *framer_state = *framer_loc;
  if (framer_state != NULL) {
    protobuf_framer_buffer_dealloc(framer_state);
    free(framer_state);
  }
  *framer_loc = NULL;
}

uint8_t protobuf_framer_process_byte(framer_protobuf_state_t *framer_state, uint8_t byte)
{
  switch (framer_state->state)
  {
  case PROTOBUF_FRAMER_STATE_IDLE:
  {
    if (byte == PROTOBUF_FRAMER_PREAMBLE1) {
      framer_state->state = PROTOBUF_FRAMER_STATE_PREAMBLE;
    }
  } break;
  case PROTOBUF_FRAMER_STATE_PREAMBLE:
  {
    if (byte == PROTOBUF_FRAMER_PREAMBLE2) {
      framer_state->count = 0;
      framer_state->state = PROTOBUF_FRAMER_STATE_LENGTH;
    } else {
      framer_state->state = PROTOBUF_FRAMER_STATE_IDLE;
    }
  } break;
  case PROTOBUF_FRAMER_STATE_LENGTH:
  {
    uint8_t *length_bytes = (uint8_t *)&framer_state->msg_length;
    length_bytes[(framer_state->count)++] = byte;
    if (framer_state->count == sizeof(uint16_t)) {
      if (framer_state->msg_length > PROTOBUF_FRAMER_BUFFER_SIZE_MAX) {
        syslog(LOG_ERR, "Protobuf framing decoded length exceeds 'safe' limit");
        framer_state->state = PROTOBUF_FRAMER_STATE_ERROR;
      } else if (framer_state->msg_length == 0) {
        framer_state->count = 0;
        framer_state->state = PROTOBUF_FRAMER_STATE_IDLE;
      } else {
        framer_state->count = 0;
        framer_state->state = PROTOBUF_FRAMER_STATE_COLLECT;
      }
    }
  } break;
  case PROTOBUF_FRAMER_STATE_COLLECT:
  {
    framer_state->frame_buffer[(framer_state->count)++] = byte;
    if (framer_state->count == framer_state->msg_length) {
      framer_state->state = PROTOBUF_FRAMER_STATE_FOUND;
    }
  } break;
  case PROTOBUF_FRAMER_STATE_FOUND:
  {
    framer_state->count = 0;
    if (byte == PROTOBUF_FRAMER_PREAMBLE1) {
      framer_state->state = PROTOBUF_FRAMER_STATE_PREAMBLE;
    }
  } break;
  case PROTOBUF_FRAMER_STATE_ERROR:
  default:
  {
    protobuf_framer_reset(framer_state);
  } break;
  }
  return framer_state->state;
}

uint32_t framer_process(void *state, const uint8_t *data, uint32_t data_length,
                        const uint8_t **frame, uint32_t *frame_length)
{
  framer_protobuf_state_t *framer_state = (framer_protobuf_state_t *)state;

  size_t max_new_count = framer_state->count + data_length;
  if (max_new_count > framer_state->frame_buffer_size) {
    protobuf_framer_buffer_alloc(framer_state, max_new_count);
  }
  uint32_t bytes_read = 0;
  while (bytes_read < data_length) {
    if (protobuf_framer_process_byte(framer_state, data[bytes_read++]) == PROTOBUF_FRAMER_STATE_FOUND) {
        *frame = framer_state->frame_buffer;
        *frame_length = framer_state->count;
        return bytes_read;
    }
  }

  *frame = NULL;
  *frame_length = 0;
  return bytes_read;
}
