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

/**
 * @file    sbp_tx.h
 * @brief   SBP TX API.
 *
 * @defgroup    sbp_tx SBP TX
 * @addtogroup  sbp_tx
 * @{
 */

#ifndef LIBPIKSI_SBP_TX_H
#define LIBPIKSI_SBP_TX_H

#include <libpiksi/common.h>

#include <libpiksi/endpoint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct  sbp_tx_ctx_t
 *
 * @brief   Opaque context for SBP TX.
 */
typedef struct sbp_tx_ctx_s sbp_tx_ctx_t;

/**
 * @brief   Create an SBP TX context.
 * @details Create and initialize an SBP TX context used to send SBP
 *          messages.
 *
 * @param[in] ident         The identity of this pub/sub pair, typically used for metrics.
 * @param[in] endpoint      Description of endpoint to connect to.
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
sbp_tx_ctx_t *sbp_tx_create(const char *ident, const char *endpoint);

/**
 * @brief   Create an SBP TX context.
 * @details Create and initialize an SBP TX context used to send SBP
 *          messages.
 *
 * @param[in] ident         The identity of this pub/sub pair, typically used for metrics.
 * @param[in] endpoint      Description of endpoint to connect to.
 * @param[in] server        True if this is a server socket.
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
sbp_tx_ctx_t *sbp_tx_create_ex(const char *ident, const char *endpoint, bool server);

/**
 * @brief   Destroy an SBP TX context.
 * @details Deinitialize and destroy an SBP TX context.
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] ctx_loc    Double pointer to the context to destroy.
 */
void sbp_tx_destroy(sbp_tx_ctx_t **ctx_loc);

/**
 * @brief   Send an SBP message.
 * @details Send an SBP message using the default SBP sender ID.
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
int sbp_tx_send(sbp_tx_ctx_t *ctx, u16 msg_type, u8 len, u8 *payload);

/**
 * @brief   Send an SBP message with non-default SBP sender ID.
 * @details Send an SBP message with non-default SBP sender ID.
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
int sbp_tx_send_from(sbp_tx_ctx_t *ctx, u16 msg_type, u8 len, u8 *payload, u16 sbp_sender_id);

/**
 * @brief Fetch the underlying endpoint for this object.
 */
pk_endpoint_t *sbp_tx_endpoint_get(sbp_tx_ctx_t *ctx);

/**
 *
 */

bool sbp_tx_dump_to_file(sbp_tx_ctx_t *ctx, const char *filename);

/**
 * @brief   Attach TX Context to a given Piksi loop
 * @details Attach TX Context to a given Piksi loop
 *
 * @param[in] ctx           Pointer to the SBP TX Context.
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 *
 * @return                  The operation result.
 * @retval 0                Context attached successfully.
 * @retval -1               An error occurred.
 */
int sbp_tx_attach(sbp_tx_ctx_t *ctx, pk_loop_t *pk_loop);

/**
 * @brief   Detach TX Context from a given Piksi loop
 * @details Detach TX Context from a given Piksi loop
 *
 * @param[in] ctx           Pointer to the SBP TX Context.
 *
 * @return                  The operation result.
 * @retval 0                Context detached successfully.
 * @retval -1               An error occurred.
 */
int sbp_tx_detach(sbp_tx_ctx_t *ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBPIKSI_SBP_TX_H */

/** @} */
