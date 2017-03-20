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

#ifndef LIBPIKSI_SBP_ZMQ_PUBSUB_H
#define LIBPIKSI_SBP_ZMQ_PUBSUB_H

#include "common.h"

#include "sbp_zmq_rx.h"
#include "sbp_zmq_tx.h"

typedef struct sbp_zmq_pubsub_ctx_s sbp_zmq_pubsub_ctx_t;

sbp_zmq_pubsub_ctx_t * sbp_zmq_pubsub_create(const char *pub_ept,
                                             const char *sub_ept);
void sbp_zmq_pubsub_destroy(sbp_zmq_pubsub_ctx_t **ctx);

sbp_zmq_tx_ctx_t * sbp_zmq_pubsub_tx_ctx_get(sbp_zmq_pubsub_ctx_t *ctx);
sbp_zmq_rx_ctx_t * sbp_zmq_pubsub_rx_ctx_get(sbp_zmq_pubsub_ctx_t *ctx);
zsock_t * sbp_zmq_pubsub_zsock_pub_get(sbp_zmq_pubsub_ctx_t *ctx);
zsock_t * sbp_zmq_pubsub_zsock_sub_get(sbp_zmq_pubsub_ctx_t *ctx);
zloop_t * sbp_zmq_pubsub_zloop_get(sbp_zmq_pubsub_ctx_t *ctx);

#endif /* LIBPIKSI_SBP_ZMQ_PUBSUB_H */
