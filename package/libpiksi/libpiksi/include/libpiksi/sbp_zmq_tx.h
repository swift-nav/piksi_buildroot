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
 * @file    sbp_zmq_tx.h
 * @brief   SBP ZMQ TX API.
 *
 * @defgroup    sbp_zmq_tx SBP ZMQ TX
 * @addtogroup  sbp_zmq_tx
 * @{
 */

#ifndef LIBPIKSI_SBP_ZMQ_TX_H
#define LIBPIKSI_SBP_ZMQ_TX_H

#include <libpiksi/common.h>

/**
 * @struct  sbp_zmq_tx_ctx_t
 *
 * @brief   Opaque context for SBP ZMQ TX.
 */
typedef struct sbp_zmq_tx_ctx_s sbp_zmq_tx_ctx_t;

/**
 * @brief   Create an SBP ZMQ TX context.
 * @details Create and initialize an SBP ZMQ TX context used to send SBP
 *          messages.
 *
 * @param[in] zsock         Pointer to the ZMQ socket to use.
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
sbp_zmq_tx_ctx_t * sbp_zmq_tx_create(zsock_t *zsock);

/**
 * @brief   Destroy an SBP ZMQ TX context.
 * @details Deinitialize and destroy an SBP ZMQ TX context.
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] ctx        Double pointer to the context to destroy.
 */
void sbp_zmq_tx_destroy(sbp_zmq_tx_ctx_t **ctx);

/**
 * @brief   Send an SBP message.
 * @details Send an SBP message via ZMQ using the default SBP sender ID.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] msg_type      Type of SBP message to send.
 * @param[in] len           Length of the data in @p payload.
 * @param[in] payload       Pointer to the SBP payload to send.
 *
 * @return                  The operation result.
 * @retval 0                The SBP message was sent successfully.
 * @retval -1               An error occurred.
 */
int sbp_zmq_tx_send(sbp_zmq_tx_ctx_t *ctx, u16 msg_type, u8 len, u8 *payload);

/**
 * @brief   Send an SBP message with non-default SBP sender ID.
 * @details Send an SBP message via ZMQ using a non-default SBP sender ID.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] msg_type      Type of SBP message to send.
 * @param[in] len           Length of the data in @p payload.
 * @param[in] payload       Pointer to the SBP payload to send.
 * @param[in] sbp_sender_id SBP sender ID to use.
 *
 * @return                  The operation result.
 * @retval 0                The SBP message was sent successfully.
 * @retval -1               An error occurred.
 */
int sbp_zmq_tx_send_from(sbp_zmq_tx_ctx_t *ctx, u16 msg_type, u8 len,
                         u8 *payload, u16 sbp_sender_id);

#endif /* LIBPIKSI_SBP_ZMQ_TX_H */

/** @} */
