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

#include <fcntl.h>
#include <limits.h>

#include <libpiksi/util.h>
#include <libpiksi/logging.h>

#include <libpiksi/sbp_rx.h>

struct sbp_rx_ctx_s {
  pk_endpoint_t *pk_ept;
  sbp_state_t sbp_state;
  const u8 *receive_buffer;
  s32 receive_buffer_length;
  bool reader_interrupt;
  void *reader_handle;
  char endpoint[PATH_MAX];
};

static s32 receive_buffer_read(u8 *buff, u32 n, void *context)
{
  sbp_rx_ctx_t *ctx = (sbp_rx_ctx_t *)context;
  u32 len = SWFT_MIN(n, ctx->receive_buffer_length);
  memcpy(buff, ctx->receive_buffer, len);
  ctx->receive_buffer += len;
  ctx->receive_buffer_length -= len;
  return len;
}

static int receive_process(const u8 *buff, size_t length, void *context)
{
  sbp_rx_ctx_t *ctx = (sbp_rx_ctx_t *)context;
  ctx->receive_buffer = buff;
  ctx->receive_buffer_length = length;

  while (ctx->receive_buffer_length > 0) {
    sbp_process(&ctx->sbp_state, receive_buffer_read);
  }

  return 0;
}

sbp_rx_ctx_t *sbp_rx_create(const char *endpoint)
{
  assert(endpoint != NULL);

  sbp_rx_ctx_t *ctx = (sbp_rx_ctx_t *)malloc(sizeof(sbp_rx_ctx_t));
  if (ctx == NULL) {
    piksi_log(LOG_ERR, "error allocating context");
    goto failure;
  }

  bzero(ctx->endpoint, sizeof(ctx->endpoint));

  ctx->pk_ept = pk_endpoint_create(endpoint, PK_ENDPOINT_SUB);
  if (ctx->pk_ept == NULL) {
    piksi_log(LOG_ERR, "error creating SUB endpoint for rx ctx");
    goto failure;
  }

  strncpy(ctx->endpoint, endpoint, sizeof(ctx->endpoint) - 1);

  ctx->reader_interrupt = false;
  ctx->reader_handle = NULL;

  sbp_state_init(&ctx->sbp_state);
  sbp_state_set_io_context(&ctx->sbp_state, ctx);

  return ctx;

failure:
  sbp_rx_destroy(&ctx);
  return NULL;
}

void sbp_rx_destroy(sbp_rx_ctx_t **ctx_loc)
{
  if (ctx_loc == NULL || *ctx_loc == NULL) {
    return;
  }
  sbp_rx_ctx_t *ctx = *ctx_loc;
  pk_endpoint_destroy(&ctx->pk_ept);
  free(ctx);
  *ctx_loc = NULL;
}

static void rx_ctx_reader_loop_callback(pk_loop_t *pk_loop, void *handle, int status, void *context)
{
  (void)handle;

  int force_new_fd = -1;

  sbp_rx_ctx_t *rx_ctx = (sbp_rx_ctx_t *)context;
  if (status & LOOP_DISCONNECTED) {

    piksi_log(LOG_WARNING, "sbp_rx: reader disconnected, reconnecting...");

    if (sbp_rx_detach(rx_ctx) < 0) {
      PK_LOG_ANNO(LOG_ERR, "error detaching endpoint from loop");
      goto fail;
    }

    /* Workaround for issue in libuv which causes a crash if the same IO handle (file descriptor
     * number) gets created for something that was just closed...
     *
     * Wisps of hints on how to fix this extracted from the following bug reports:
     *   - https://github.com/libuv/libuv/issues/1495
     *   - https://github.com/nodejs/node-v0.x-archive/issues/4558
     */
    force_new_fd = open("/dev/null", O_RDWR);

    /* Socket was disconnected, reconnect */
    pk_endpoint_destroy(&rx_ctx->pk_ept);
    rx_ctx->pk_ept = pk_endpoint_create(rx_ctx->endpoint, PK_ENDPOINT_SUB);

    if (rx_ctx->pk_ept == NULL) {
      PK_LOG_ANNO(LOG_ERR, "error creating SUB endpoint");
      goto fail;
    }

    if (sbp_rx_attach(rx_ctx, pk_loop) < 0) {
      PK_LOG_ANNO(LOG_ERR, "error re-attaching endpoint to loop");
      goto fail;
    }

    close(force_new_fd);
  }

  sbp_rx_reader_interrupt_reset(rx_ctx);
  int rc = sbp_rx_read(rx_ctx);

  if (rc != 0) {
    PK_LOG_ANNO(LOG_WARNING, "sbp_rx_read failed, rc = %d", rc);
  }

  if (sbp_rx_reader_interrupt_requested(rx_ctx)) {
    pk_loop_stop(pk_loop);
  }

fail:
  if (force_new_fd != -1) close(force_new_fd);
}

int sbp_rx_attach(sbp_rx_ctx_t *ctx, pk_loop_t *pk_loop)
{
  assert(ctx != NULL);
  assert(pk_loop != NULL);

  ctx->reader_handle =
    pk_loop_endpoint_reader_add(pk_loop, ctx->pk_ept, rx_ctx_reader_loop_callback, ctx);

  if (ctx->reader_handle == NULL) {
    piksi_log(LOG_ERR, "error adding rx_ctx reader to loop");
    return -1;
  }

  return 0;
}

int sbp_rx_detach(sbp_rx_ctx_t *ctx)
{
  assert(ctx != NULL);

  if (ctx->reader_handle == NULL) {
    return 0; // Nothing to do here
  }

  if (pk_loop_remove_handle(ctx->reader_handle) != 0) {
    piksi_log(LOG_ERR, "error removing reader from loop");
    return -1;
  }

  ctx->reader_handle = NULL;
  return 0;
}

int sbp_rx_callback_register(sbp_rx_ctx_t *ctx,
                             u16 msg_type,
                             sbp_msg_callback_t cb,
                             void *context,
                             sbp_msg_callbacks_node_t **node)
{
  assert(ctx != NULL);

  sbp_msg_callbacks_node_t *n = (sbp_msg_callbacks_node_t *)malloc(sizeof(*n));
  if (n == NULL) {
    piksi_log(LOG_ERR, "error allocating callback node");
    return -1;
  }

  if (sbp_register_callback(&ctx->sbp_state, msg_type, cb, context, n) != SBP_OK) {
    piksi_log(LOG_ERR, "error registering SBP callback");
    free(n);
    return -1;
  }

  if (node != NULL) {
    *node = n;
  }

  return 0;
}

int sbp_rx_callback_remove(sbp_rx_ctx_t *ctx, sbp_msg_callbacks_node_t **node)
{
  assert(ctx != NULL);
  assert(node != NULL);
  assert(*node != NULL);

  if (sbp_remove_callback(&ctx->sbp_state, *node) != SBP_OK) {
    piksi_log(LOG_ERR, "error removing SBP callback");
    return -1;
  }

  free(*node);
  *node = NULL;
  return 0;
}

int sbp_rx_read(sbp_rx_ctx_t *ctx)
{
  assert(ctx != NULL);

  return pk_endpoint_receive(ctx->pk_ept, receive_process, ctx);
}

void sbp_rx_reader_interrupt(sbp_rx_ctx_t *ctx)
{
  assert(ctx != NULL);

  ctx->reader_interrupt = true;
}

void sbp_rx_reader_interrupt_reset(sbp_rx_ctx_t *ctx)
{
  assert(ctx != NULL);

  ctx->reader_interrupt = false;
}

bool sbp_rx_reader_interrupt_requested(sbp_rx_ctx_t *ctx)
{
  assert(ctx != NULL);

  return ctx->reader_interrupt;
}
