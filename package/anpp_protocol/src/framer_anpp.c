/*
 * Copyright (C) 2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "an_packet_protocol.h"

typedef struct {
  an_decoder_t an_decoder;
  an_packet_t *an_packet;
} framer_anpp_state_t;

static void framer_state_free_packet(framer_anpp_state_t *s)
{
  if (s != NULL) {
    if (s->an_packet != NULL) {
      an_packet_free(&s->an_packet);
    }
  }
}

void *framer_create(void)
{
  framer_anpp_state_t *s = calloc(1, sizeof(*s));
  if (s == NULL) {
    return NULL;
  }

  an_decoder_initialise(&s->an_decoder);
  s->an_packet = NULL; // explicitly clear
  return (void *)s;
}

void framer_destroy(void **state)
{
  if (state != NULL) {
    framer_anpp_state_t *s = (framer_anpp_state_t *)(*state);
    framer_state_free_packet(s); // clear lingering packets
  }
  free(*state);
  *state = NULL;
}

uint32_t framer_process(void *state,
                        const uint8_t *data,
                        uint32_t data_length,
                        const uint8_t **frame,
                        uint32_t *frame_length)
{
  framer_anpp_state_t *s = (framer_anpp_state_t *)state;
  framer_state_free_packet(s); // clear any stale packets
  *frame = NULL;
  *frame_length = 0;

  if (data_length == 0) {
    return 0; // process called with no data
  }

  an_decoder_t *an_decoder = &s->an_decoder;
  uint32_t read_length =
    SWFT_MIN(SWFT_MIN(data_length, AN_MAXIMUM_PACKET_SIZE), an_decoder_size(an_decoder));
  if (read_length == 0) {
    sbp_log(LOG_ERR, "ANPP Decoder ran out of room, clearing packet data");
    an_decoder_initialise(an_decoder);
    return 0;
  }

  memcpy(an_decoder_pointer(an_decoder), data, read_length);
  an_decoder_increment(an_decoder, read_length);
  s->an_packet = an_packet_decode(an_decoder); // save result for release
  if (s->an_packet != NULL) {
    *frame = an_packet_pointer(s->an_packet);
    *frame_length = an_packet_size(s->an_packet);
    return read_length;
  }

  return read_length;
}
