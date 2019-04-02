/*
 * Copyright (C) 2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swift-nav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_IO_THREAD_H
#define SWIFTNAV_IO_THREAD_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 */
typedef struct io_thread_s io_thread_t;

typedef void (* io_thread_work_fn_t)(void *context);

/**
 * @brief
 */
typedef struct {
  size_t max_pending;   /**< maximum number of pending io operations */
  sbp_tx_ctx_t *tx_ctx; /**< The context to use for outgoing data */
} io_thread_cfg_t;

/**
 * @brief
 */
io_thread_t *io_thread_create(io_thread_cfg_t *cfg);

/**
 * @brief
 */
bool io_thread_setup_metrics(path_validator_t *ctx, const char *name);

/**
 * @brief
 */
void io_thread_destroy(io_thread_t **pctx);

/**
 * @brief 
 */
bool io_thread_start(io_thread_t *ctx);

/**
 * @brief 
 */
bool io_thread_stop(io_thread_t *ctx);

/**
 * @brief
 */
bool io_thread_enque(io_thread_t *io_ctx, io_thread_work_fn_t *work_fn, void *ctx);

/**
 * @brief
 */
bool io_thread_flush_output_sbp(io_thread_t *io_ctx);

#ifdef __cplusplus
}
#endif

#endif // SWIFTNAV_IO_THREAD_H
