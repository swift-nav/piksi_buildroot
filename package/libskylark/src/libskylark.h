/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_LIBSKYLARK_H
#define SWIFTNAV_LIBSKYLARK_H

#include <stdint.h>

typedef enum {
  E_NO_ERROR             =  0,   /** No error occurred! */
  E_GENERIC_ERROR        = -1,   /** Generic error. */
  E_NULL_VALUE_ERROR     = -2,   /** A required arguments was null.*/
  E_DEVICE_CONN_ERROR    = -3,   /** Error connecting to device. */
  E_DEVICE_CONF_ERROR    = -4,   /** Error configuring device. */
  E_CONNECTION_ERROR     = -5,   /** Connection failed. */
  E_SUBSCRIBE_ERROR      = -6,   /** Subscribe failed. */
  E_PUBLISH_ERROR        = -7,   /** Publish failed. */
  E_DISCONNECT_ERROR     = -8,   /** Disconnect failed. */
  E_SSL_CONNECT_ERROR    = -9,   /** TLS handshake failed. */
  E_SSL_CERT_ERROR       = -10,  /** SSL certificate error */
  E_UNSUBSCRIBE_ERROR    = -11,  /** Unsubscribe failed. */
  E_UNSUPPORTED_PLATFORM = -12   /** Unsupported platform. */
} skylark_error_t;

// TLS support with self-signed CA
#define SKIP_PEER_VERIFICATION 1
#define SKIP_HOST_VERIFICATION 1

// Status codes
#define PUT_STATUS_OK          200
#define GET_STATUS_OK          202

// Header definitions
#define ENCODING                  "Transfer-Encoding: chunked"
#define SBP_V2_ACCEPT_TYPE        "Accept: application/vnd.swiftnav.broker.v1+sbp2"
#define SBP_V2_CONTENT_TYPE       "Content-Type: application/vnd.swiftnav.broker.v1+sbp2"
#define DEFAULT_DEVICE_UID        "Device-Uid: aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa"
#define DEFAULT_CHANNEL_UID       "Device-Uid: aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa"
#define DEFAULT_BROKER_ENDPOINT   "https://broker.staging.skylark.swiftnav.com"
#define USER_AGENT                "libskylark-agent/1.0"

#endif /* SWIFTNAV_LIBSKYLARK_H */
