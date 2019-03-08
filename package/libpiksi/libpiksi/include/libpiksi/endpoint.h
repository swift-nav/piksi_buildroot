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
#include <libpiksi/loop.h>

#define PK_ENDPOINT_RECV_BUF_SIZE (8 * 1024)

#ifdef __cplusplus
extern "C" {
#endif

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
  PK_ENDPOINT_INVALID = -1,
  PK_ENDPOINT_PUB,
  PK_ENDPOINT_PUB_SERVER,
  PK_ENDPOINT_SUB,
  PK_ENDPOINT_SUB_SERVER,
  PK_ENDPOINT_REP,
  PK_ENDPOINT_REQ
} pk_endpoint_type;

enum {
  PKE_SUCCESS = 0,
  PKE_ERROR = -1,
  PKE_NOT_CONN = -2,
  PKE_EAGAIN = -3,
};

typedef struct {
  /**
   * The address for the endpoint, for pk_endpoint, currently only unix domain sockets
   * are supported, so an address must be specified as "ipc:///path/to/socket.foo"
   */
  const char *endpoint;
  /**
   * The identity of this socket, used for metrics and to give a server socket a
   * means to record the identity of incoming connections.
   */
  const char *identity;
  /**
   * The type of this socket, see @c pk_endpoint_type.
   */
  pk_endpoint_type type;
  /**
   * If the endpoint should try to reconnect when starting.
   */
  bool retry_connect;
} pk_endpoint_config_t;

typedef struct pk_endpoint_config_builder_s pk_endpoint_config_builder_t;

/**
 * A "builder pattern" object for constructing an endpoint config.
 */
struct pk_endpoint_config_builder_s {

  pk_endpoint_config_t _config;

  /**
   * Set the endpoint path/address on the config.
   */
  pk_endpoint_config_builder_t (*endpoint)(const char *endpoint);

  /**
   * Set the endpoint identity, usually used for metrics.
   */
  pk_endpoint_config_builder_t (*identity)(const char *identity);

  /**
   * Set the endpoint type in the config.
   */
  pk_endpoint_config_builder_t (*type)(pk_endpoint_type type);

  /**
   * Set the 'retry_connect' parameter in the config, tells the endpoint to retry connecting during
   * create.
   */
  pk_endpoint_config_builder_t (*retry_connect)(bool retry_connect);

  /**
   * Returns a filled @c pk_endpoint_config_t object.
   */
  pk_endpoint_config_t (*get)(void);
};

/**
 * @brief   Piksi Endpoint Receive Callback Signature
 */
typedef int (*pk_endpoint_receive_cb)(const u8 *data, const size_t length, void *context);

pk_endpoint_config_builder_t pk_endpoint_config(void);

/**
 * @brief   Create a Piksi Endpoint context
 * @details Create a Piksi Endpoint context
 *
 * @param[in] cfg           The config to use, see @c pk_endpoint_config_t
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
pk_endpoint_t *pk_endpoint_create(pk_endpoint_config_t cfg);

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
 * @brief   Get the type of an endpoint context
 * @details Get the type of an endpoint context
 *
 * @param[in] pk_ept        Pointer to Piksi endpoint context to use.
 *
 * @return                  Type of endpoint
 */
pk_endpoint_type pk_endpoint_type_get(pk_endpoint_t *pk_ept);

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
 * @brief   Read a single message from the endpoint context into a supplied buffer
 * @details Read a single message from the endpoint context into a supplied buffer
 *
 * @param[in] pk_ept        Pointer to Piksi endpoint context to use.
 * @param[out] buffer       Pointer the memory location the message will be copied to.
 * @param[in] count         Size of the supplied buffer.
 *
 * @return                  The operation result.
 * @retval 0                Number of bytes copied into the supplied buffer.
 * @retval -1               An error occurred.
 */
ssize_t pk_endpoint_read(pk_endpoint_t *pk_ept, u8 *buffer, size_t count);

/**
 * @brief   Receive messages from the endpoint context
 * @details Receive messages from the endpoint context. The callback supplied
 *          will be called for each message received. A single call to this function
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
int pk_endpoint_send(pk_endpoint_t *pk_ept, const u8 *data, size_t length);

/**
 * @brief   Get specific error string following and operation that failed
 * @details Get specific error string following and operation that failed
 *
 * @return                  Pointer to a const string buffer with description
 */
const char *pk_endpoint_strerror(void);

/**
 * @brief For server sockets, accept an incoming connection.
 */
int pk_endpoint_accept(pk_endpoint_t *pk_ept);

/**
 * @brief Configure a socket to be non-blocking
 */
int pk_endpoint_set_non_blocking(pk_endpoint_t *pk_ept);

/**
 * @brief   Add a server socket to a loop for servicing.
 *
 * @details
 *   Server sockets created with @c pk_endpoint_create need to be added
 *   to a loop in order to be serviced.  Other sockets may need this
 *   in the future to support things like reconnection/retransmission.
 *
 *   For client sockets (PUB/SUB/REQ):
 *
 *   - *PUB*: these sockets do not produce data (data is only sent /
 *     published to a server *SUB* socket)-- so they do not need to be
 *     associatedwith a loop except for (later) we may use the loop
 *     to do things like automatic reconnect.
 *
 *     TODO: for certain degenerate cases, a server could conceivably
 *     write data to the PUB socket, a loop would be required to discard
 *     this data or we could investigate using `shutdown()` to close
 *     down the read side of the socket.
 *
 *   - *SUB*: the caller should service the data form this socket by
 *     registering a reader with @c pk_loop_endpoint_reader_add and then
 *     calling @c pk_endpoint_receive to receive data.  Client *SUB*
 *     sockets are connected to server *PUB* sockets and receive data
 *     published from other clients.
 *
 *   - *REQ*: similar to *PUB*
 *
 *   For server sockets (PUB_SERVER/SUB_SERVER/REP):
 *
 *   - *PUB_SERVER*: A loop is required in order to service new connections,
 *     no data should be written to pub sockets, so any data written will
 *     wake the event loop but it will be read and discarded.
 *
 *     TODO: investigate if we can use `shutdown()` to avoid having to
 *     read and discard data.
 *
 *   - *SUB_SERVER*: A loop is required in order to service new connections,
 *     the caller should register to receive data by using @c pk_loop_endpoint_reader_add
 *     and calling @c pk_endpoint_poll_handle_get to get the handle that
 *     will wake when any client writes data.
 *
 *   - *REP*: similar to *PUB_SERVER*
 *
 * @param[in] pk_ept        Pointer to Piksi endpoint context to use
 * @param[in] loop          The loop with which to associate
 *
 * @return  Zero on success, -1 on failure
 */
int pk_endpoint_loop_add(pk_endpoint_t *pk_ept, pk_loop_t *loop);

/**
 * Validate that the current socket handle associated with this endpoint is
 * still valid.
 */
bool pk_endpoint_is_valid(pk_endpoint_t *pk_ept);

typedef void (*pk_endpoint_eagain_fn_t)(pk_endpoint_t *pk_ept, size_t bytes);

/**
 * Set callback for notifying an endpoint client that a data drop has
 * happened.
 *
 * In pk_endpoint_t we attempt to enforce latency over throughput by only
 * allowing clients of server sockets to block for a certain amount of time
 * (10ms currently).  If a client's in kernel buffer fills up (at around 160k)
 * then we'll receive an EAGAIN error on a call to `sendmsg()` even though
 * we're in non-blocking mode, this signals that a client is not emptying it's
 * buffer -- we allow this state for 10ms before we close the socket to flush the
 * in-kernel buffer.  Using `pk_endpoint_eagain_cb_set` a user of pk_endpoint_t
 * can receive a notification that such a drop has occurred.
 */
void pk_endpoint_eagain_cb_set(pk_endpoint_t *pk_ept, pk_endpoint_eagain_fn_t eagain_cb);

#ifdef __cplusplus
}
#endif

#endif /* LIBPIKSI_ENDPOINT_H */

/** @} */
