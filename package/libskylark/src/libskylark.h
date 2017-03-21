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
#include <stdio.h>
// #include <sbp_settings.h>
#include <sbp_zmq.h>

/**
 *  Logging utilities.
 */

#define log_info(...) fprintf(stderr, __VA_ARGS__)
#define log_debug(...) fprintf(stderr, __VA_ARGS__)
#define log_warn(...) fprintf(stderr, __VA_ARGS__)
#define log_error(...) fprintf(stderr, __VA_ARGS__)

/**
 *  Constant definitions
 */

typedef enum {
  NO_ERROR               =  0,   /** No error occurred! */
  E_NOT_IMPLEMENTED      =  1,   /** Not implemented */
  E_GENERIC_ERROR        =  2,   /** Generic error. */
  E_NULL_VALUE_ERROR     =  3,   /** A required arguments was null.*/
  E_DEVICE_CONN_ERROR    =  4,   /** Error connecting to device. */
  E_DEVICE_CONF_ERROR    =  5,   /** Error configuring device. */
  E_CONNECTION_ERROR     =  6,   /** Connection failed. */
  E_SUBSCRIBE_ERROR      =  7,   /** Subscribe failed. */
  E_PUBLISH_ERROR        =  8,   /** Publish failed. */
  E_DISCONNECT_ERROR     =  9,   /** Disconnect failed. */
  E_SSL_CONNECT_ERROR    =  10,  /** TLS handshake failed. */
  E_SSL_CERT_ERROR       =  11,  /** SSL certificate error */
  E_UNSUBSCRIBE_ERROR    =  12,  /** Unsubscribe failed. */
  E_UNSUPPORTED_PLATFORM =  13   /** Unsupported platform. */
} SKYLARK_RC;

/**
 *  Constant definitions: libcurl configuration and Skylark status codes.
 */

// TLS support with self-signed CA
#define SKIP_PEER_VERIFICATION 1
#define SKIP_HOST_VERIFICATION 1

// HTTP Status codes
typedef enum {
  STATUS_PUT_OK             = 200,
  STATUS_GET_OK             = 202,
  STATUS_NO_HEADER          = 400,
  STATUS_BAD_HEADER         = 400,
  STATUS_NO_ROVER_POSITION  = 404,
  STATUS_SOURCE_EXIT        = 400,
  STATUS_BASE_EXIT          = 500,
  STATUS_PATH_NOT_FOUND     = 404,
  STATUS_METHOD_NOT_FOUND   = 400,
} skylark_response_status_t;


// Header definitions
#define STREAM_ENCODING         "Transfer-Encoding: chunked"
#define SBP_V2_ACCEPT_TYPE      "Accept: application/vnd.swiftnav.broker.v1+sbp2"
#define SBP_V2_CONTENT_TYPE     "Content-Type: application/vnd.swiftnav.broker.v1+sbp2"
#define RTCM_V3_CONTENT_TYPE    "Content-Type: application/vnd.swiftnav.broker.v1+rtcm3"
#define DEVICE_UID_HEADER_FMT   "Device-Uid: %s"
#define DEFAULT_CHANNEL_UID     "118db405-b5de-4a05-87b5-605cc85af924"
#define DEFAULT_DEVICE_UID      "22222222-2222-2222-2222-222222222222"
#define DEFAULT_BROKER_ENDPOINT "https://broker.skylark2.swiftnav.com"
#define USER_AGENT              "libskylark-agent/1.0"

/**
 *  Constant definitions: Skylark - Piksi settings groups and boilerplate.
 */

// TODO (mookerji): Finish these
#define SETTINGS_SKYLARK_GROUP  "skylark"
#define SETTINGS_SKYLARK_ENABLE "enable"
#define SETTINGS_SKYLARK_URL    "url"

/* static const char const * skylark_mode_enum[] = {"ON", "OFF", NULL}; */
/* static struct setting_type skylark_mode_settings_type; */
/* static int TYPE_SKYLARK_MODE = 0; */
/* enum {SKYLARK_ENABLED, SKYLARK_DISABLED}; */

/**
 *  Constant definitions: Common type definitions around configuration and
 *  header files.
 */

// TODO (mookerji): Document these
typedef struct {
  const char *endpoint_url;
  char *accept_type_header;
  char *content_type_header;
  char *encoding;
  char *device_uuid;
  char *device_header;
  u16 sbp_sender_id;
  int fd_in;
  int fd_out;
  u8 enabled;
  sbp_zmq_config_t *sbp_zmq_config;
} skylark_client_config_t;

void log_client_config(const skylark_client_config_t *config);

/**
 *  Settings and HTTP client configuration setup.
 */

SKYLARK_RC setup_settings(skylark_client_config_t* config);

SKYLARK_RC get_uuid_channel(skylark_client_config_t* config);
SKYLARK_RC get_device_uuid(skylark_client_config_t* config);
SKYLARK_RC get_broker_endpoint(skylark_client_config_t* config);
SKYLARK_RC format_device_header(skylark_client_config_t* config);
SKYLARK_RC get_sbp_sender_id(u16* sender_id);
SKYLARK_RC get_config(skylark_client_config_t* config);

/**
 *  Download (i.e., rover) processes.
 */

SKYLARK_RC download_process(skylark_client_config_t* config);
SKYLARK_RC download_io_loop(skylark_client_config_t* config);

/**
 *  Upload processes, for base stations and reference station processing.
 */

SKYLARK_RC upload_process(skylark_client_config_t *config);
SKYLARK_RC upload_io_loop(skylark_client_config_t *config);

#endif /* SWIFTNAV_LIBSKYLARK_H */
