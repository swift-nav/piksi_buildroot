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

#ifndef LIBPIKSI_SBP_RX_H
#define LIBPIKSI_SBP_RX_H

#include <libpiksi/common.h>

#include <libpiksi/endpoint.h>
#include <libpiksi/loop.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sbp_rx_ctx_s sbp_rx_ctx_t;

/**
 * @struct  sbp_rx_ctx_t
 *
 * @brief   Opaque context for SBP RX.
 */
typedef struct sbp_rx_ctx_s sbp_rx_ctx_t;

/**
 * @brief   Create an SBP RX context.
 * @details Create and initialize an SBP RX context used to receive SBP
 *          messages.
 *
 * @param[in] endpoint      Description of endpoint to connect to.
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
sbp_rx_ctx_t *sbp_rx_create(const char *endpoint);

/**
 * @brief   Destroy an SBP RX context.
 * @details Deinitialize and destroy an SBP RX context.
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] ctx_loc    Double pointer to the context to destroy.
 */
void sbp_rx_destroy(sbp_rx_ctx_t **ctx_loc);

/**
 * @brief   Attach RX Context to a given Piksi loop
 * @details Attach RX Context to a given Piksi loop
 *
 * @param[in] ctx           Pointer to the SBP RX Context.
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 *
 * @return                  The operation result.
 * @retval 0                Context attached successfully.
 * @retval -1               An error occurred.
 */
int sbp_rx_attach(sbp_rx_ctx_t *ctx, pk_loop_t *pk_loop);

/**
 * @brief   Detach RX Context from a given Piksi loop
 * @details Detach RX Context from a given Piksi loop
 *
 * @param[in] ctx           Pointer to the SBP RX Context.
 *
 * @return                  The operation result.
 * @retval 0                Context detached successfully.
 * @retval -1               An error occurred.
 */
int sbp_rx_detach(sbp_rx_ctx_t *ctx);

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
 *                          using sbp_rx_callback_remove(). May be
 *                          NULL if unused.
 *
 * @return                  The operation result.
 * @retval 0                The callback was registered successfully.
 * @retval -1               An error occurred.
 */
int sbp_rx_callback_register(sbp_rx_ctx_t *ctx,
                             u16 msg_type,
                             sbp_msg_callback_t cb,
                             void *context,
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
int sbp_rx_callback_remove(sbp_rx_ctx_t *ctx, sbp_msg_callbacks_node_t **node);

/**
 * @brief   Read and process incoming data.
 * @details Read and process all incoming messages.
 *
 * @note    This function will block until a message is received, and will
 *          process multiple messages if more than one has been queued.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  The operation result.
 * @retval 0                A message was successfully read and processed.
 * @retval -1               An error occurred.
 */
int sbp_rx_read(sbp_rx_ctx_t *ctx);

/**
 * @brief   Request interrupt of RX reader added loops.
 * @details Sets an interrupt that will be caught by loops implementing the
 *          interrupt_requested check.
 *
 * @note    This function may be called at any time, but it is intended to be
 *          called from an SBP message callback when a loop is used to
 *          receive messages.
 *
 * @param[in] ctx           Pointer to the context to use.
 */
void sbp_rx_reader_interrupt(sbp_rx_ctx_t *ctx);

/**
 * @brief   Reset reader interrupt.
 *
 * @param[in] ctx           Pointer to the context to use.
 */
void sbp_rx_reader_interrupt_reset(sbp_rx_ctx_t *ctx);

/**
 * @brief   Status of interrupt request.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return bool             True if the interrupt was requested.
 */
bool sbp_rx_reader_interrupt_requested(sbp_rx_ctx_t *ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBPIKSI_SBP_RX_H */

/** @} */
