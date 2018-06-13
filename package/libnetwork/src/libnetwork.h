/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file    libnetwork.h
 * @brief   Network API.
 *
 * @defgroup    network Network
 * @addtogroup  network
 * @{
 */

#ifndef SWIFTNAV_LIBNETWORK_H
#define SWIFTNAV_LIBNETWORK_H

#include <stdbool.h>

#define SKYLARK_REQ_FIFO_NAME "/var/run/skylark/control/dl.req"
#define SKYLARK_REP_FIFO_NAME "/var/run/skylark/control/dl.resp"

typedef struct {
  const char* req_fifo_name;
  const char* rep_fifo_name;
} control_pair_t;

#define SKYLARK_CONTROL_PAIR (control_pair_t){ SKYLARK_REQ_FIFO_NAME, SKYLARK_REP_FIFO_NAME }

#define CONTROL_COMMAND_STATUS "s"

typedef struct network_context_s network_context_t;

/**
 * @brief   Standard settings type definitions.
 */
typedef enum {
  NETWORK_TYPE_INVALID,          /**< Context for an ntrip download session  */
  NETWORK_TYPE_NTRIP_DOWNLOAD,   /**< Context for an ntrip download session  */
  NETWORK_TYPE_SKYLARK_UPLOAD,   /**< Context for a skylark upload session   */
  NETWORK_TYPE_SKYLARK_DOWNLOAD, /**< Context for a skylark download session */
} network_type_t;

/**
 * @brief   Error type
 */
typedef enum {
  NETWORK_STATUS_INVALID_SETTING    = -1, /** < The setting is invalid for this type   */
  NETWORK_STATUS_URL_TOO_LARGE      = -2, /** < The URL specified is too large         */
  NETWORK_STATUS_USERNAME_TOO_LARGE = -3, /** < The username specified is too large    */
  NETWORK_STATUS_PASSWORD_TOO_LARGE = -4, /** < The password specified is too large    */
  NETWORK_STATUS_FIFO_ERROR         = -5, /** < There was an error creating a FIFO     */
  NETWORK_STATUS_WRITE_ERROR        = -6, /** < There was an error writing to a FIFO   */
  NETWORK_STATUS_READ_ERROR         = -7, /** < There was an error reading from a FIFO */
  NETWORK_STATUS_SUCCESS            =  0,  /** < The operation was successful          */
} network_status_t;

/**
 * @brief Create a context for a libnetwork session
 *
 * @param[in] type          The type of libnetwork session, @see @c network_type_t
 *
 * @return                  The network_context_t that was created, NULL on error.
 */
network_context_t* libnetwork_create(network_type_t type);

/**
 * @brief   Destroy a libnetwork context.
 * @details Deinitialize and destroy a libnetwork context.
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] ctx        Double pointer to the context to destroy.
 */
void libnetwork_destroy(network_context_t **ctx);

/**
 * @brief Set the FD for this context
 *
 * @return                   The operation result.  See @ref network_status_t.
 */
network_status_t libnetwork_set_fd(network_context_t* context, int fd);

/**
 * @brief Set the username for this context
 *
 * @return                   The operation result.  See @ref network_status_t.
 */
network_status_t libnetwork_set_username(network_context_t* context, const char* username);

/**
 * @brief Set the password for this context
 *
 * @return                   The operation result.  See @ref network_status_t.
 */
network_status_t libnetwork_set_password(network_context_t* context, const char* password);

/**
 * @brief Set the url for this context
 *
 * @return                   The operation result.  See @ref network_status_t.
 */
network_status_t libnetwork_set_url(network_context_t* context, const char* url);

/**
 * @brief Set the debug flag for this context
 *
 * @return                   The operation result.  See @ref network_status_t.
 */
network_status_t libnetwork_set_debug(network_context_t* context, bool debug);

/**
 * @brief Set the NTRIP GGA upload frequency for this context, this is only valid for session created with type LIBNETWORK_NTRIP_DOWNLOAD
 *
 * @param[in] gga_interval   The GGA upload interval in seconds.
 *
 * @return                   The operation result.  See @ref network_status_t.
 */
network_status_t libnetwork_set_gga_upload_interval(network_context_t* context, int gga_interval);

/**
 * @brief Set the NTRIP GGA upload style to NTRIP 1.0
 *
 * @param[in] use_rev1       If true, the NTRIP GGA string will be formatted according to NTRIP 1.0
 *
 * @return                   The operation result.  See @ref network_status_t.
 */
network_status_t libnetwork_set_gga_upload_rev1(network_context_t* context, bool use_rev1);

/**
 * @brief   Download from ntrip.
 * @details Download observations and other messages from a CORS station.
 *
 * @param[in] config        Pointer to the config to use.
 */
void ntrip_download(network_context_t *ctx);

/**
 * @brief   Download from skylark.
 * @details Download observations and other messages from skylark.
 *
 * @param[in] config        Pointer to the config to use.
 */
void skylark_download(network_context_t *ctx);

/**
 * @brief   Upload to skylark.
 * @details Upload observations and other messages to skylark.
 *
 * @param[in] config        Pointer to the config to use.
 */
void skylark_upload(network_context_t *ctx);

/**
 * @brief Graceful termination handler for libnetwork daemons.
 *
 * @details Grafcefully stop the upload/download loops started by
 * ntrip_download, skylark_download, or skylark_upload.
 */
void libnetwork_shutdown(void);

/**
 * @brief Cycle (reconnect) the current network connection
 */
void libnetwork_cycle_connection(void);

/**
 * @brief Configures whether libnetwork should report errors (defaults to true).
 */
void libnetwork_report_errors(network_context_t *ctx, bool yesno);

/**
 * @brief Configures the request and response control FIFOs
 */
network_status_t libnetwork_configure_control(network_context_t *ctx, control_pair_t control_pair);

/**
 * @brief Configures the request and response control FIFOs
 */
network_status_t libnetwork_request_health(control_pair_t control_pair, int *status);

#endif /* SWIFTNAV_LIBNETWORK_H */
