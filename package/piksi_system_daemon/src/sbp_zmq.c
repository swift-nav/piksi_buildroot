/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libsbp/sbp.h>
#include <czmq.h>

#include <stdlib.h>

#include "sbp_zmq.h"

static u16 sender_id = SBP_SENDER_ID;

static struct sbp_zmq_ctx {
  zsock_t *pub, *sub;
  zloop_t *loop;
  u8 send_buf[256];
  u8 send_len;
  u8 *recv_buf;
  u8 recv_len;
} sbp_zmq_ctx;

static int reader_fn(zloop_t *loop, zsock_t *reader, void *arg);

static u32 sbp_write(u8 *buf, u32 n, void *context)
{
  struct sbp_zmq_ctx *ctx = context;
  n = MIN(n, sizeof(ctx->send_buf));
  memcpy(&ctx->send_buf[ctx->send_len], buf, n);
  ctx->send_len += n;
  return n;
}

static void sbp_write_flush(struct sbp_zmq_ctx *ctx)
{
  zmsg_t *msg = zmsg_new();
  zmsg_addmem(msg, ctx->send_buf, ctx->send_len);
  zmsg_print(msg);
  zmsg_send(&msg, ctx->pub);
  ctx->send_len = 0;
}

void sbp_zmq_send_msg(sbp_state_t *sbp, u16 msg_type, u8 len, u8 buff[])
{
  struct sbp_zmq_ctx *ctx = sbp->io_context;
  sbp_send_message(sbp, msg_type, sender_id, len, buff, sbp_write);
  sbp_write_flush(ctx);
}

static u32 sbp_read(u8 *buff, u32 n, void *context)
{
  struct sbp_zmq_ctx *ctx = context;
  size_t len = MIN(n, ctx->recv_len);
  memcpy(buff, ctx->recv_buf, len);
  ctx->recv_buf += len;
  ctx->recv_len -= len;
  return len;
}

void sbp_zmq_process(sbp_state_t *sbp)
{
  struct sbp_zmq_ctx *ctx = sbp->io_context;
  zmsg_t *msg = zmsg_recv(ctx->sub);
  for (zframe_t *frame = zmsg_first(msg); frame; frame = zmsg_next(msg)) {
    ctx->recv_buf = zframe_data(frame);
    ctx->recv_len = zframe_size(frame);
    while (ctx->recv_len)
      sbp_process(sbp, sbp_read);
  }
  zmsg_destroy(&msg);
}

s8 sbp_zmq_register_callback(sbp_state_t *s, u16 msg_type, sbp_msg_callback_t cb)
{
  sbp_msg_callbacks_node_t *node = malloc(sizeof(*node));
  return sbp_register_callback(s, msg_type, cb, s, node);
}

sbp_state_t *sbp_zmq_init(void)
{
  sbp_state_t *sbp = malloc(sizeof(*sbp));
  struct sbp_zmq_ctx *ctx = malloc(sizeof(*ctx));

  /* Setup ZMQ sockets and SBP state */
  sbp_state_init(sbp);
  ctx->pub =  zsock_new_pub(">tcp://localhost:43011");
  ctx->sub = zsock_new_sub(">tcp://localhost:43010", "");
  sbp_state_set_io_context(sbp, ctx);

  ctx->loop = zloop_new();
  zloop_reader(ctx->loop, ctx->sub, reader_fn, sbp);

  return sbp;
}

static int reader_fn(zloop_t *loop, zsock_t *reader, void *arg)
{
  (void)loop; (void)reader;
  sbp_zmq_process(arg);
  return 0;
}

static int timeout_fn(zloop_t *loop, int timer, void *arg)
{
  (void)loop;
  (void)timer;
  (void)arg;
  /* Break the loop */
  return -1;
}

struct wait_closure {
  bool ret;
  zloop_t *loop;
};

static void sbp_zmq_wait_msg_cb(u16 sender_id, u8 len, u8 msg[], void* context)
{
  (void)sender_id;
  (void)len;
  (void)msg;
  struct wait_closure *ctx = context;
  ctx->ret = true;
  /* Trigger a timeout immediately to exit the loop */
  zloop_timer(ctx->loop, 0, 1, timeout_fn, NULL);
}

bool sbp_zmq_wait_msg(sbp_state_t *sbp, u16 msg_type, size_t timeout)
{
  struct sbp_zmq_ctx *ctx = sbp->io_context;
  struct wait_closure closure = {0};

  sbp_msg_callbacks_node_t node;
  sbp_register_callback(sbp, msg_type, sbp_zmq_wait_msg_cb, &closure, &node);

  /* Run message handler loop */
  zloop_t *loop = zloop_new();
  closure.loop = loop;
  zloop_timer(loop, timeout, 1, timeout_fn, NULL);
  zloop_start(loop);

  sbp_remove_callback(sbp, &node);

  return closure.ret;
}

void sbp_zmq_loop(sbp_state_t *sbp)
{
  struct sbp_zmq_ctx *ctx = sbp->io_context;

  /* Run message handler loop */
  zloop_start(ctx->loop);

  /* Cleanup */
  zsock_destroy(&ctx->pub);
  zsock_destroy(&ctx->sub);
  while(sbp->sbp_msg_callbacks_head) {
    sbp_msg_callbacks_node_t *n = sbp->sbp_msg_callbacks_head;
    sbp->sbp_msg_callbacks_head = n->next;
    free(n);
  }
  free(ctx);
  free(sbp);
}

zloop_t *sbp_zmq_get_loop(sbp_state_t *sbp)
{
  struct sbp_zmq_ctx *ctx = sbp->io_context;
  return ctx->loop;
}
