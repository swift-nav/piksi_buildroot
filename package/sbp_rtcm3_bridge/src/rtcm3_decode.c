/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "rtcm3_decode.h"
#include "sbp.h"
#include <stdio.h>

void rtcm3_decode_frame(const uint8_t *frame, uint32_t frame_length)
{
  static uint32_t count = 0;
  uint16_t message_type = (frame[3] << 4) | ((frame[4] >> 4) & 0xf);
  printf("message type: %u, length: %u, count: %u\n",
          message_type, frame_length, ++count);
}
