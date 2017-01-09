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

#include "sbp_zmq.h"

#include <stdio.h>

typedef struct {
  u8 send_buffer[264];
  u32 send_buffer_length;
  const u8 *receive_buffer;
  u32 receive_buffer_length;
} sbp_context_t;

static void sbp_send_buffer_reset(sbp_zmq_state_t *s)
{
  sbp_context_t *c = (sbp_context_t *)s->sbp_state->io_context;
  c->send_buffer_length = 0;
}

static u32 sbp_send_buffer_write(u8 *buff, u32 n, void *context)
{
  sbp_context_t *c = (sbp_context_t *)context;
  u32 len = MIN(sizeof(c->send_buffer) - c->send_buffer_length, n);
  memcpy(&c->send_buffer[c->send_buffer_length], buff, len);
  c->send_buffer_length += len;
  return len;
}

static int sbp_send_buffer_flush(sbp_zmq_state_t *s)
{
  sbp_context_t *c = (sbp_context_t *)s->sbp_state->io_context;

  zmsg_t *msg = zmsg_new();
  if (msg == NULL) {
    printf("error in zmsg_new()\n");
    return -1;
  }

  if (zmsg_addmem(msg, c->send_buffer, c->send_buffer_length) != 0) {
    printf("error in zmsg_addmem()\n");
    zmsg_destroy(&msg);
    return -1;
  }

  if (zmsg_send(&msg, s->pub) != 0) {
    printf("error in zmsg_send()\n");
    return -1;
  }

  return 0;
}

static u32 sbp_receive_buffer_read(u8 *buff, u32 n, void *context)
{
  sbp_context_t *c = (sbp_context_t *)context;
  u32 len = MIN(n, c->receive_buffer_length);
  memcpy(buff, c->receive_buffer, len);
  c->receive_buffer += len;
  c->receive_buffer_length -= len;
  return len;
}

static int zloop_reader_handler(zloop_t *loop, zsock_t *sub, void *arg)
{
  (void)loop;
  sbp_zmq_state_t *s = (sbp_zmq_state_t *)arg;
  sbp_context_t *c = (sbp_context_t *)s->sbp_state->io_context;

  s->interrupt = false;

  zmsg_t *msg = zmsg_recv(sub);
  if (msg == NULL) {
    printf("error in zmsg_recv()\n");
    return -1;
  }

  zframe_t *frame;
  for (frame = zmsg_first(msg); frame != NULL; frame = zmsg_next(msg)) {
    c->receive_buffer = zframe_data(frame);
    c->receive_buffer_length = zframe_size(frame);
    while (c->receive_buffer_length > 0) {
      sbp_process(s->sbp_state, sbp_receive_buffer_read);
    }
  }

  zmsg_destroy(&msg);

  /* If a callback requested an interrupt, return -1 to break the ZMQ loop */
  return s->interrupt ? -1 : 0;
}

static int zloop_timer_handler(zloop_t *loop, int timer_id, void *arg)
{
  (void)loop;
  (void)timer_id;
  (void)arg;

  return -1;
}

int sbp_zmq_init(sbp_zmq_state_t *s, const sbp_zmq_config_t *config)
{
  s->sbp_state = malloc(sizeof(*s->sbp_state));
  if (s->sbp_state == NULL) {
    printf("error allocating SBP state\n");
    return -1;
  }

  sbp_state_init(s->sbp_state);
  s->sbp_sender_id = config->sbp_sender_id;

  sbp_context_t *sbp_context = malloc(sizeof(*sbp_context));
  if (sbp_context == NULL) {
    printf("error allocating SBP context\n");
    return -1;
  }

  sbp_state_set_io_context(s->sbp_state, sbp_context);

  s->pub = zsock_new_pub(config->pub_endpoint);
  if (s->pub == NULL) {
    printf("error creating PUB socket\n");
    return -1;
  }

  s->sub = zsock_new_sub(config->sub_endpoint, "");
  if (s->sub == NULL) {
    printf("error creating SUB socket\n");
    return -1;
  }

  s->zloop = zloop_new();
  if (s->zloop == NULL) {
    printf("error creating zloop\n");
    return -1;
  }

  if (zloop_reader(s->zloop, s->sub, zloop_reader_handler, s) != 0) {
    printf("error adding zloop reader\n");
    return -1;
  }

  return 0;
}

int sbp_zmq_deinit(sbp_zmq_state_t *s)
{
  while(s->sbp_state->sbp_msg_callbacks_head) {
    sbp_msg_callbacks_node_t *node = s->sbp_state->sbp_msg_callbacks_head;
    s->sbp_state->sbp_msg_callbacks_head = node->next;
    free(node);
  }
  free(s->sbp_state->io_context);
  free(s->sbp_state);
  zsock_destroy(&s->pub);
  zsock_destroy(&s->sub);
  zloop_destroy(&s->zloop);
  return 0;
}

int sbp_zmq_callback_register(sbp_zmq_state_t *s, u16 msg_type,
                              sbp_msg_callback_t cb, void *context,
                              sbp_msg_callbacks_node_t **node)
{
  sbp_msg_callbacks_node_t *n = malloc(sizeof(*n));
  if (n == NULL) {
    printf("error allocating callback node\n");
    return -1;
  }

  if (sbp_register_callback(s->sbp_state, msg_type,
                            cb, context, n) != SBP_OK) {
    printf("error registering SBP callback\n");
    free(n);
    return -1;
  }

  if (node != NULL) {
    *node = n;
  }
  return 0;
}

int sbp_zmq_callback_remove(sbp_zmq_state_t *s,
                            sbp_msg_callbacks_node_t *node)
{
  if (sbp_remove_callback(s->sbp_state, node) != SBP_OK) {
    printf("error removing SBP callback\n");
    return -1;
  }

  free(node);

  return 0;
}

int sbp_zmq_message_send(sbp_zmq_state_t *s, u16 msg_type, u8 len, u8 *payload)
{
  sbp_send_buffer_reset(s);
  if (sbp_send_message(s->sbp_state, msg_type, s->sbp_sender_id, len, payload,
                       sbp_send_buffer_write) != SBP_OK) {
    printf("error sending SBP message\n");
    return -1;
  }

  return sbp_send_buffer_flush(s);
}

int sbp_zmq_loop(sbp_zmq_state_t *s)
{
  int zloop_ret = zloop_start(s->zloop);
  if (zloop_ret == -1) {
    /* Canceled by a handler */
    return 0;
  } else {
    /* Interrupted or an error occurred */
    return -1;
  }
}

int sbp_zmq_loop_timeout(sbp_zmq_state_t *s, u32 timeout_ms)
{
  int timer_id = zloop_timer(s->zloop, timeout_ms, 1,
                             zloop_timer_handler, s);

  if (timer_id < 0) {
    printf("error creating zloop timer\n");
    return -1;
  }

  int result = sbp_zmq_loop(s);
  zloop_timer_end(s->zloop, timer_id);
  return result;
}

int sbp_zmq_loop_interrupt(sbp_zmq_state_t *s)
{
  s->interrupt = true;
  return 0;
}

zloop_t *sbp_zmq_loop_get(sbp_zmq_state_t *s)
{
  return s->zloop;
}
