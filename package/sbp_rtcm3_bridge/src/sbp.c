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

#include "sbp.h"
#include <czmq.h>
#include <stdio.h>

typedef struct {
  u8 send_buffer[264];
  u32 send_buffer_length;
} sbp_context_t;

static sbp_state_t sbp_state;
static sbp_context_t sbp_context;
static u16 sbp_sender_id;
static zsock_t *sbp_pub;

static void sbp_send_buffer_reset(void)
{
  sbp_context.send_buffer_length = 0;
}

static u32 sbp_send_buffer_write(u8 *buff, u32 n, void *context)
{
  u32 len = MIN(sizeof(sbp_context.send_buffer) -
                sbp_context.send_buffer_length, n);
  memcpy(&sbp_context.send_buffer[sbp_context.send_buffer_length], buff, len);
  sbp_context.send_buffer_length += len;
  return len;
}

static int sbp_send_buffer_flush(void)
{
  zmsg_t *msg = zmsg_new();
  if (msg == NULL) {
    printf("error in zmsg_new()\n");
    return -1;
  }

  if (zmsg_addmem(msg, sbp_context.send_buffer,
                  sbp_context.send_buffer_length) != 0) {
    printf("error in zmsg_addmem()\n");
    zmsg_destroy(&msg);
    return -1;
  }

  if (zmsg_send(&msg, sbp_pub) != 0) {
    printf("error in zmsg_send()\n");
    return -1;
  }

  return 0;
}

int sbp_init(u16 sender_id, const char *pub_endpoint)
{
  sbp_state_init(&sbp_state);
  sbp_sender_id = sender_id;

  sbp_pub = zsock_new_pub(pub_endpoint);
  if (sbp_pub == NULL) {
    printf("error creating PUB socket\n");
    return -1;
  }

  return 0;
}

int sbp_message_send(u16 msg_type, u8 len, u8 *payload)
{
  sbp_send_buffer_reset();
  if (sbp_send_message(&sbp_state, msg_type, sbp_sender_id, len, payload,
                       sbp_send_buffer_write) != SBP_OK) {
    printf("error sending SBP message\n");
    return -1;
  }

  return sbp_send_buffer_flush();
}
