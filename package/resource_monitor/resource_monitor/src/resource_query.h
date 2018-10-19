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
 * @file    resource_query.h
 * @brief   Piksi Resource Daemon.
 *
 * @defgroup    resource_query
 * @addtogroup  resource_query
 * @{
 */

#ifndef RESOURCE_QUERY_H
#define RESOURCE_QUERY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libpiksi/common.h>

#define SBP_PAYLOAD_SIZE_MAX (255u)

typedef void *(*resq_init_fn_t)();
typedef const char *(*resq_describe_fn_t)(void);
typedef void (*resq_run_query_fn_t)(void *context);
typedef bool (*resq_prepare_sbp_fn_t)(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context);
typedef void (*resq_teardown_fn_t)(void **context);

/**
 * @brief Teardown the resource_query object
 */

typedef struct {
  /**
   * @brief Initialize the resource query object.
   *
   * @return a context that will be passed to subsequent method invocations.
   */
  resq_init_fn_t init;

  /**
   * @brief Run the resource query.
   */
  resq_describe_fn_t describe;

  /**
   * @brief Run the resource query.
   */
  resq_run_query_fn_t run_query;

  /**
   * @brief Prepare an SBP packet
   */
  resq_prepare_sbp_fn_t prepare_sbp;

  /**
   * @brief Teardown the query object
   */
  resq_teardown_fn_t teardown;

} resq_interface_t;

/**
 * @brief Register a query object
 */
void resq_register(resq_interface_t *resq);

/**
 * @brief Run all query objects
 */
void resq_run_all(void);

/**
 * @brief Destroy a query object
 */
void resq_destroy_all(void);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_QUERY_H */

/** @} */
