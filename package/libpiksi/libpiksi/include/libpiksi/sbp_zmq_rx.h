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
 * @file    sbp_zmq_rx.h
 * @brief   SBP ZMQ RX API.
 *
 * @defgroup    sbp_zmq_rx SBP ZMQ RX
 * @addtogroup  sbp_zmq_rx
 * @{
 */

#ifndef LIBPIKSI_SBP_ZMQ_RX_H
#define LIBPIKSI_SBP_ZMQ_RX_H

#include <libpiksi/common.h>

/**
 * @struct  sbp_zmq_rx_ctx_t
 *
 * @brief   Opaque context for SBP ZMQ RX.
 */
typedef struct sbp_zmq_rx_ctx_s sbp_zmq_rx_ctx_t;

/**
 * @brief   Create an SBP ZMQ RX context.
 * @details Create and initialize an SBP ZMQ RX context used to receive SBP
 *          messages.
 *
 * @param[in] zsock         Pointer to the ZMQ socket to use.
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
sbp_zmq_rx_ctx_t * sbp_zmq_rx_create(zsock_t *zsock);

/**
 * @brief   Destroy an SBP ZMQ RX context.
 * @details Deinitialize and destroy an SBP ZMQ RX context.
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] ctx        Double pointer to the context to destroy.
 */
void sbp_zmq_rx_destroy(sbp_zmq_rx_ctx_t **ctx);

/**
 * @brief   Register an SBP message callback.
 * @details Register a callback function to be executed when an SBP message of
 *          the specified type has been received.
 *
 * @see     libsbp, @c <libsbp/sbp.h>.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] msg_type      Type of SBP message to listen for.
 * @param[in] cb            SBP callback function to execute.
 * @param[in] context       SBP callback context.
 * @param[out] node         Double pointer to be set to the allocated SBP
 *                          callback node. Required to remove the callback
 *                          using sbp_zmq_rx_callback_remove(). May be
 *                          NULL if unused.
 *
 * @return                  The operation result.
 * @retval 0                The callback was registered successfully.
 * @retval -1               An error occurred.
 */
int sbp_zmq_rx_callback_register(sbp_zmq_rx_ctx_t *ctx, u16 msg_type,
                                 sbp_msg_callback_t cb, void *context,
                                 sbp_msg_callbacks_node_t **node);

/**
 * @brief   Remove an SBP message callback.
 * @details Remove a registered SBP message callback.
 *
 * @note    The node pointer will be set to NULL by this function on success.
 *
 * @see     libsbp, @c <libsbp/sbp.h>.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[inout] node       Double pointer to the SBP callback node to remove.
 *
 * @return                  The operation result.
 * @retval 0                The callback was removed successfully.
 * @retval -1               An error occurred.
 */
int sbp_zmq_rx_callback_remove(sbp_zmq_rx_ctx_t *ctx,
                               sbp_msg_callbacks_node_t **node);

/**
 * @brief   Read and process incoming data.
 * @details Read and process a single incoming ZMQ message.
 *
 * @note    This function will block until a ZMQ message is received. For
 *          nonblocking operation, use the pollitem or reader APIs.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  The operation result.
 * @retval 0                A message was successfully read and processed.
 * @retval -1               An error occurred.
 */
int sbp_zmq_rx_read(sbp_zmq_rx_ctx_t *ctx);

/**
 * @brief   Initialize a ZMQ pollitem.
 * @details Initialize a ZMQ pollitem to be used to poll the associated ZMQ
 *          socket for pending messages.
 *
 * @see     czmq, @c zmq_poll().
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[out] pollitem     Pointer to the ZMQ pollitem to initialize.
 *
 * @return                  The operation result.
 * @retval 0                The ZMQ pollitem was initialized successfully.
 * @retval -1               An error occurred.
 */
int sbp_zmq_rx_pollitem_init(sbp_zmq_rx_ctx_t *ctx, zmq_pollitem_t *pollitem);

/**
 * @brief   Check a ZMQ pollitem.
 * @details Check a ZMQ pollitem for pending messages and read a single
 *          incoming ZMQ message from the associated socket if available.
 *
 * @see     czmq, @c zmq_poll().
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] pollitem      Pointer to the ZMQ pollitem to check.
 *
 * @return                  The operation result.
 * @retval 0                The ZMQ pollitem was checked successfully.
 * @retval -1               An error occurred.
 */
int sbp_zmq_rx_pollitem_check(sbp_zmq_rx_ctx_t *ctx, zmq_pollitem_t *pollitem);

/**
 * @brief   Add a reader to a ZMQ loop.
 * @details Add a reader for the associated socket to a ZMQ loop. The reader
 *          will be executed to process pending messages when available.
 *
 * @note    Pending messages will only be processed while the ZMQ loop is
 *          running. See @c zloop_start().
 *
 * @see     czmq, @c zloop_start().
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] zloop         Pointer to the ZMQ loop to use.
 *
 * @return                  The operation result.
 * @retval 0                The reader was added successfully.
 * @retval -1               An error occurred.
 */
int sbp_zmq_rx_reader_add(sbp_zmq_rx_ctx_t *ctx, zloop_t *zloop);

/**
 * @brief   Remove a reader from a ZMQ loop.
 * @details Remove a reader for the associated socket from a ZMQ loop.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] zloop         Pointer to the ZMQ loop to use.
 *
 * @return                  The operation result.
 * @retval 0                The reader was removed successfully.
 * @retval -1               An error occurred.
 */
int sbp_zmq_rx_reader_remove(sbp_zmq_rx_ctx_t *ctx, zloop_t *zloop);

/**
 * @brief   Interrupt a ZMQ loop.
 * @details Interrupt a ZMQ loop by returning -1 from the associated reader.
 *
 * @note    This function may be called at any time, but it is intended to be
 *          called from an SBP message callback when a ZMQ loop is used to
 *          receive messages.
 *
 * @see     czmq, @c zloop_start().
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  The operation result.
 * @retval 0                The ZMQ loop was interrupted successfully.
 * @retval -1               An error occurred.
 */
int sbp_zmq_rx_reader_interrupt(sbp_zmq_rx_ctx_t *ctx);

#endif /* LIBPIKSI_SBP_ZMQ_RX_H */

/** @} */
