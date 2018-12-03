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
 * @file    sbp_pubsub.h
 * @brief   SBP PUB/SUB API.
 *
 * @defgroup    sbp_pubsub SBP PUB/SUB
 * @addtogroup  sbp_pubsub
 * @{
 */

#ifndef LIBPIKSI_SBP_PUBSUB_H
#define LIBPIKSI_SBP_PUBSUB_H

#include <libpiksi/common.h>

#include <libpiksi/sbp_rx.h>
#include <libpiksi/sbp_tx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct  sbp_pubsub_ctx_t
 *
 * @brief   Opaque context for SBP PUB/SUB.
 */
typedef struct sbp_pubsub_ctx_s sbp_pubsub_ctx_t;

/**
 * @brief   Create an SBP PUB/SUB context.
 * @details Create and initialize an SBP PUB/SUB context. This is a
 *          helper module used to set up a typical configuration consisting of
 *          the following:
 *          @li SBP TX context
 *          @li SBP RX context
 *          @li SBP PUB socket, associated with the TX context
 *          @li SBP SUB socket, associated with the RX context
 *
 * @param[in] ident         The identity of this pub/sub pair, typically used for metrics.
 * @param[in] pub_ept       String describing the SBP PUB endpoint to use.
 * @param[in] sub_ept       String describing the SBP SUB endpoint to use.
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
sbp_pubsub_ctx_t *sbp_pubsub_create(const char *ident, const char *pub_ept, const char *sub_ept);

/**
 * @brief   Destroy an SBP PUB/SUB context.
 * @details Deinitialize and destroy an SBP PUB/SUB context.
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] ctx_loc        Double pointer to the context to destroy.
 */
void sbp_pubsub_destroy(sbp_pubsub_ctx_t **ctx_loc);

/**
 * @brief   Retrieve the SBP TX context.
 * @details Retrieve the SBP TX context associated with the specified
 *          PUB/SUB context.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  Pointer to the SBP TX context.
 */
sbp_tx_ctx_t *sbp_pubsub_tx_ctx_get(sbp_pubsub_ctx_t *ctx);

/**
 * @brief   Retrieve the SBP RX context.
 * @details Retrieve the SBP RX context associated with the specified
 *          PUB/SUB context.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  Pointer to the SBP RX context.
 */
sbp_rx_ctx_t *sbp_pubsub_rx_ctx_get(sbp_pubsub_ctx_t *ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBPIKSI_SBP_PUBSUB_H */

/** @} */
