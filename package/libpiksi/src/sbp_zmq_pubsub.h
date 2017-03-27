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

/**
 * @file    sbp_zmq_pubsub.h
 * @brief   SBP ZMQ PUB/SUB API.
 *
 * @defgroup    sbp_zmq_pubsub SBP ZMQ PUB/SUB
 * @addtogroup  sbp_zmq_pubsub
 * @{
 */

#ifndef LIBPIKSI_SBP_ZMQ_PUBSUB_H
#define LIBPIKSI_SBP_ZMQ_PUBSUB_H

#include "common.h"

#include "sbp_zmq_rx.h"
#include "sbp_zmq_tx.h"

/**
 * @struct  sbp_zmq_pubsub_ctx_t
 *
 * @brief   Opaque context for SBP ZMQ PUB/SUB.
 */
typedef struct sbp_zmq_pubsub_ctx_s sbp_zmq_pubsub_ctx_t;

/**
 * @brief   Create an SBP ZMQ PUB/SUB context.
 * @details Create and initialize an SBP ZMQ PUB/SUB context. This is a
 *          helper module used to set up a typical configuration consisting of
 *          the following:
 *          @li SBP ZMQ TX context
 *          @li SBP ZMQ RX context
 *          @li ZMQ PUB socket, associated with the TX context
 *          @li ZMQ SUB socket, associated with the RX context
 *          @li ZMQ loop, with a reader for the RX context aready configured
 *
 * @param[in] pub_ept       String describing the ZMQ PUB endpoint to use.
 * @param[in] sub_ept       String describing the ZMQ SUB endpoint to use.
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
sbp_zmq_pubsub_ctx_t * sbp_zmq_pubsub_create(const char *pub_ept,
                                             const char *sub_ept);

/**
 * @brief   Destroy an SBP ZMQ PUB/SUB context.
 * @details Deinitialize and destroy an SBP ZMQ PUB/SUB context.
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] ctx        Double pointer to the context to destroy.
 */
void sbp_zmq_pubsub_destroy(sbp_zmq_pubsub_ctx_t **ctx);

/**
 * @brief   Retrieve the SBP ZMQ TX context.
 * @details Retrieve the SBP ZMQ TX context associated with the specified
 *          PUB/SUB context.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  Pointer to the SBP ZMQ TX context.
 */
sbp_zmq_tx_ctx_t * sbp_zmq_pubsub_tx_ctx_get(sbp_zmq_pubsub_ctx_t *ctx);

/**
 * @brief   Retrieve the SBP ZMQ RX context.
 * @details Retrieve the SBP ZMQ RX context associated with the specified
 *          PUB/SUB context.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  Pointer to the SBP ZMQ RX context.
 */
sbp_zmq_rx_ctx_t * sbp_zmq_pubsub_rx_ctx_get(sbp_zmq_pubsub_ctx_t *ctx);

/**
 * @brief   Retrieve the ZMQ PUB socket.
 * @details Retrieve the ZMQ PUB socket associated with the specified
 *          PUB/SUB context.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  Pointer to the ZMQ PUB socket.
 */
zsock_t * sbp_zmq_pubsub_zsock_pub_get(sbp_zmq_pubsub_ctx_t *ctx);

/**
 * @brief   Retrieve the ZMQ SUB socket.
 * @details Retrieve the ZMQ SUB socket associated with the specified
 *          PUB/SUB context.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  Pointer to the ZMQ SUB socket.
 */
zsock_t * sbp_zmq_pubsub_zsock_sub_get(sbp_zmq_pubsub_ctx_t *ctx);

/**
 * @brief   Retrieve the ZMQ loop.
 * @details Retrieve the ZMQ loop associated with the specified
 *          PUB/SUB context.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  Pointer to the ZMQ loop.
 */
zloop_t * sbp_zmq_pubsub_zloop_get(sbp_zmq_pubsub_ctx_t *ctx);

#endif /* LIBPIKSI_SBP_ZMQ_PUBSUB_H */

/** @} */
