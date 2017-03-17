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

#ifndef LIBPIKSI_SBP_ZMQ_RX_H
#define LIBPIKSI_SBP_ZMQ_RX_H

#include "common.h"

typedef struct sbp_zmq_rx_ctx_s sbp_zmq_rx_ctx_t;

sbp_zmq_rx_ctx_t * sbp_zmq_rx_create(zsock_t *zsock);
void sbp_zmq_rx_destroy(sbp_zmq_rx_ctx_t **ctx);

int sbp_zmq_rx_callback_register(sbp_zmq_rx_ctx_t *ctx, u16 msg_type,
                                 sbp_msg_callback_t cb, void *context,
                                 sbp_msg_callbacks_node_t **node);
int sbp_zmq_rx_callback_remove(sbp_zmq_rx_ctx_t *ctx,
                               sbp_msg_callbacks_node_t **node);

int sbp_zmq_rx_read(sbp_zmq_rx_ctx_t *ctx);
int sbp_zmq_rx_pollitem_init(sbp_zmq_rx_ctx_t *ctx, zmq_pollitem_t *pollitem);
int sbp_zmq_rx_pollitem_check(sbp_zmq_rx_ctx_t *ctx, zmq_pollitem_t *pollitem);
int sbp_zmq_rx_reader_add(sbp_zmq_rx_ctx_t *ctx, zloop_t *zloop);
int sbp_zmq_rx_reader_remove(sbp_zmq_rx_ctx_t *ctx, zloop_t *zloop);
int sbp_zmq_rx_reader_interrupt(sbp_zmq_rx_ctx_t *ctx);

#endif /* LIBPIKSI_SBP_ZMQ_RX_H */
