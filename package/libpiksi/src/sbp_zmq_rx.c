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

#include "sbp_zmq_rx.h"
#include "util.h"
#include <stdio.h>
#include <assert.h>

struct sbp_zmq_rx_ctx_s {
  zsock_t *zsock;
  sbp_state_t sbp_state;
  const u8 *receive_buffer;
  u32 receive_buffer_length;
  bool reader_interrupt;
};

static u32 receive_buffer_read(u8 *buff, u32 n, void *context)
{
  sbp_zmq_rx_ctx_t *ctx = (sbp_zmq_rx_ctx_t *)context;
  u32 len = MIN(n, ctx->receive_buffer_length);
  memcpy(buff, ctx->receive_buffer, len);
  ctx->receive_buffer += len;
  ctx->receive_buffer_length -= len;
  return len;
}

static void receive_process(sbp_zmq_rx_ctx_t *ctx, const u8 *buff, u32 length)
{
  ctx->receive_buffer = buff;
  ctx->receive_buffer_length = length;

  while (ctx->receive_buffer_length > 0) {
    sbp_process(&ctx->sbp_state, receive_buffer_read);
  }
}

static int message_receive(sbp_zmq_rx_ctx_t *ctx)
{
  zmsg_t *msg;
  while (1) {
    msg = zmsg_recv(ctx->zsock);
    if (msg != NULL) {
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      printf("error in zmsg_recv()\n");
      return -1;
    }
  }

  zframe_t *frame;
  for (frame = zmsg_first(msg); frame != NULL; frame = zmsg_next(msg)) {
    receive_process(ctx, zframe_data(frame), zframe_size(frame));
  }

  zmsg_destroy(&msg);
  return 0;
}

static int zloop_reader_handler(zloop_t *zloop, zsock_t *zsock, void *arg)
{
  sbp_zmq_rx_ctx_t *ctx = (sbp_zmq_rx_ctx_t *)arg;

  ctx->reader_interrupt = false;

  int ret = message_receive(ctx);
  if (ret != 0) {
    return ret;
  }

  /* If a callback requested an interrupt, return -1 to break the ZMQ loop */
  return ctx->reader_interrupt ? -1 : 0;
}

sbp_zmq_rx_ctx_t * sbp_zmq_rx_create(zsock_t *zsock)
{
  assert(zsock != NULL);
  assert(zsock_is(zsock));

  sbp_zmq_rx_ctx_t *ctx = (sbp_zmq_rx_ctx_t *)malloc(sizeof(*ctx));
  if (ctx == NULL) {
    printf("error allocating context\n");
    return ctx;
  }

  ctx->zsock = zsock;
  ctx->reader_interrupt = false;

  sbp_state_init(&ctx->sbp_state);
  sbp_state_set_io_context(&ctx->sbp_state, ctx);

  return ctx;
}

void sbp_zmq_rx_destroy(sbp_zmq_rx_ctx_t **ctx)
{
  assert(ctx != NULL);
  assert(*ctx != NULL);

  free(*ctx);
  *ctx = NULL;
}

int sbp_zmq_rx_callback_register(sbp_zmq_rx_ctx_t *ctx, u16 msg_type,
                                 sbp_msg_callback_t cb, void *context,
                                 sbp_msg_callbacks_node_t **node)
{
  assert(ctx != NULL);

  sbp_msg_callbacks_node_t *n = (sbp_msg_callbacks_node_t *)malloc(sizeof(*n));
  if (n == NULL) {
    printf("error allocating callback node\n");
    return -1;
  }

  if (sbp_register_callback(&ctx->sbp_state, msg_type,
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

int sbp_zmq_rx_callback_remove(sbp_zmq_rx_ctx_t *ctx,
                               sbp_msg_callbacks_node_t **node)
{
  assert(ctx != NULL);
  assert(node != NULL);
  assert(*node != NULL);

  if (sbp_remove_callback(&ctx->sbp_state, *node) != SBP_OK) {
    printf("error removing SBP callback\n");
    return -1;
  }

  free(*node);
  *node = NULL;
  return 0;
}

int sbp_zmq_rx_read(sbp_zmq_rx_ctx_t *ctx)
{
  assert(ctx != NULL);

  int ret = message_receive(ctx);
  if (ret != 0) {
    return ret;
  }

  return 0;
}

int sbp_zmq_rx_pollitem_init(sbp_zmq_rx_ctx_t *ctx, zmq_pollitem_t *pollitem)
{
  assert(ctx != NULL);
  assert(pollitem != NULL);

  *pollitem = (zmq_pollitem_t) {
    .socket = ctx->zsock,
    .fd = 0,
    .events = ZMQ_POLLIN,
    .revents = 0
  };

  return 0;
}

int sbp_zmq_rx_pollitem_check(sbp_zmq_rx_ctx_t *ctx, zmq_pollitem_t *pollitem)
{
  assert(ctx != NULL);
  assert(pollitem != NULL);
  assert(pollitem->socket == ctx->zsock);

  if (pollitem->revents & ZMQ_POLLIN) {
    return message_receive(ctx);
  }

  return 0;
}

int sbp_zmq_rx_reader_add(sbp_zmq_rx_ctx_t *ctx, zloop_t *zloop)
{
  assert(ctx != NULL);
  assert(zloop != NULL);

  return zloop_reader(zloop, ctx->zsock, zloop_reader_handler, ctx);
}

int sbp_zmq_rx_reader_remove(sbp_zmq_rx_ctx_t *ctx, zloop_t *zloop)
{
  assert(ctx != NULL);
  assert(zloop != NULL);

  zloop_reader_end(zloop, ctx->zsock);
  return 0;
}

int sbp_zmq_rx_reader_interrupt(sbp_zmq_rx_ctx_t *ctx)
{
  assert(ctx != NULL);

  ctx->reader_interrupt = true;
  return 0;
}
