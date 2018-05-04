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
 * @file    endpoint.h
 * @brief   Piksi Endpoint API.
 *
 * @defgroup    pk_endpoint Piksi Endpoint
 * @addtogroup  pk_endpoint
 * @{
 */

#ifndef LIBPIKSI_ENDPOINT_H
#define LIBPIKSI_ENDPOINT_H

#include <libpiksi/common.h>

/**
 * @struct  pk_endpoint_t
 *
 * @brief   Opaque handle for Piksi Endpoint.
 */
typedef struct pk_endpoint_s pk_endpoint_t;

/**
 * @brief   Endpoint types supported
 */
typedef enum {
  PK_ENDPOINT_PUB,
  PK_ENDPOINT_SUB,
  PK_ENDPOINT_REP,
  PK_ENDPOINT_REQ
} pk_endpoint_type;

/**
 * @brief   Piksi Endpoint Receive Callback Signature
 */
typedef int (*pk_endpoint_receive_cb)(const u8 *data, const size_t length, void *context);

/**
 * @brief   Create a Piksi Endpoint context
 * @details Create a Piksi Endpoint context
 *
 * @param[in] endpoint      Description of the endpoint that will be connected to.
 * @param[in] type          The type of endpoint to create.
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
pk_endpoint_t * pk_endpoint_create(const char *endpoint, pk_endpoint_type type);

/**
 * @brief   Destroy a Piksi Endpoint context
 * @details Destroy a Piksi Endpoint context
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] pk_ept_loc Double pointer to the context to destroy.
 */
void pk_endpoint_destroy(pk_endpoint_t **pk_ept_loc);

/**
 * @brief   Get the poll handle of an endpoint context
 * @details Get the poll handle of an endpoint context. The main purpose
 *          of this function is for use with a pk_loop_t context
 *
 * @param[in] pk_ept        Pointer to Piksi endpoint context to use.
 *
 * @return                  Underlying file descriptor of endpoint
 */
int pk_endpoint_poll_handle_get(pk_endpoint_t *pk_ept);

/**
 * @brief   Receive messages from the endpoint context
 * @details Receive messages from the endpoint context. The callback supplied
 *          will be call for each message received. A single call to this function
 *          may result in several calls to the callback as multiple messages may
 *          be queued.
 *
 * @param[in] pk_ept        Pointer to Piksi endpoint context to use.
 * @param[in] rx_cb         Callback used to process each message.
 * @param[in] context       Userdata to be passed into the provided callback.
 *
 * @return                  The operation result.
 * @retval 0                Receive operation was successful.
 * @retval -1               An error occurred.
 */
int pk_endpoint_receive(pk_endpoint_t *pk_ept, pk_endpoint_receive_cb rx_cb, void *context);

/**
 * @brief   Send a message from an endpoint
 * @details Send a message from an endpoint. Create the message and flushes immediately.
 *
 * @param[in] pk_ept        Pointer to Piksi endpoint context to use.
 * @param[in] data          Pointer to the message data to be sent.
 * @param[in] length        Total length of the data to be sent.
 *
 * @return                  The operation result.
 * @retval 0                Send operation was successful.
 * @retval -1               An error occurred.
 */
int pk_endpoint_send(pk_endpoint_t *pk_ept, const u8 *data, const size_t length);

#endif /* LIBPIKSI_ENDPOINT_H */
