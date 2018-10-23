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

typedef enum {
  RESQ_PRIORIRTY_1,
  RESQ_PRIORIRTY_2,
  RESQ_PRIORIRTY_3,
  RESQ_PRIORIRTY_4,
  RESQ_PRIORIRTY_COUNT,
} resq_priority_t;

typedef enum {
  RESQ_PROP_NONE,
  RESQ_PROP_U8,
  RESQ_PROP_U16,
  RESQ_PROP_U32,
  RESQ_PROP_U64,
  RESQ_PROP_F64,
  RESQ_PROP_STR,
} resq_property_t;

typedef struct {
  int id;
  resq_property_t type;
  union {
    u8 u8;
    u16 u16;
    u32 u32;
    u64 u64;
    double f64;
    char *str;
  } property;
} resq_read_property_t;


/**
 * @brief Initialize the resource query object.
 *
 * @details Initialize the resource query object, returning NULL indicates that
 * the initialization failed and the query will be skipped.
 *
 * @return a context that will be passed to subsequent method invocations.
 */
typedef void *(*resq_init_fn_t)();

/**
 * @brief Provides a textual description of the resource query object, should
 * be unique.
 *
 * @details The resource query object should retain ownership of pointer.
 */
typedef const char *(*resq_describe_fn_t)(void);

/**
 * @brief Run the resource query.
 */
typedef void (*resq_run_query_fn_t)(void *context);

/**
 * @brief Prepares an SBP packet for transmission.
 *
 * @return true if more SBP packets need to be sent, false otherwise.
 */
typedef bool (*resq_prepare_sbp_fn_t)(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context);

/**
 * @brief Allow a property to be read from another module.
 *
 * @param read_prop[inout] The property to read, the .id and .type fields must be populated.
 * @param context[in]      A context to passs to the property read function.
 *
 * @return true if the property query succeeded
 */
typedef bool (*resq_read_property_fn_t)(resq_read_property_t *read_prop, void *context);

typedef void (*resq_teardown_fn_t)(void **context);

/**
 * @brief Teardown the resource_query object
 */
typedef struct {
  /**
   * @brief Function pointer that initializes the resource query object, can
   * be NULL.
   *
   * @details @see @c resq_init_fn_t
   */
  resq_init_fn_t init;

  /**
   * @brief Fuction pointer that provides a unique textual description, cannot
   * be NULL.
   *
   * @details @see @c resq_describe_fn_t
   */
  resq_describe_fn_t describe;

  /**
   * @brief Run the resource query, cannot be NULL.
   *
   * @details @see @c resq_run_query_fn_t
   */
  resq_run_query_fn_t run_query;

  /**
   * @brief Function pointer that prepare an SBP packet, cannot be NULL.
   *
   * @details @see @c resq_run_query_fn_t
   */
  resq_prepare_sbp_fn_t prepare_sbp;

  /**
   * @brief Read a property from the query module
   *
   * @details @see @c resq_read_property_fn_t
   */
  resq_read_property_fn_t read_property;

  /**
   * @brief Teardown the query object
   */
  resq_teardown_fn_t teardown;

  resq_priority_t priority;

} resq_interface_t;

/**
 * @brief Register a query object
 */
void resq_register(resq_interface_t *resq);

/**
 * @brief Run all initializer functions in priority order
 */
void resq_initilize_all(void);

/**
 * @brief Run all query objects
 */
void resq_run_all(void);

/**
 * @brief Destroy a query object
 */
void resq_destroy_all(void);

/**
 * @brief Read a property from a named resource query object
 */
bool resq_read_property(const char *query_name, resq_read_property_t *read_prop);

#ifdef __cplusplus
}
#endif

#endif /* RESOURCE_QUERY_H */

/** @} */
