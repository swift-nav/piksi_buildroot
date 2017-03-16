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
#include <string.h>
// #include <sbp_settings.h>
#include <sbp_zmq.h>

/**
 *  Logging utilities.
 */

#define log_info(...) fprintf(stdout, __VA_ARGS__)
#define log_debug(...) fprintf(stderr, __VA_ARGS__)
#define log_warn(...) fprintf(stderr, __VA_ARGS__)
#define log_error(...) fprintf(stderr, __VA_ARGS__)

/**
 *  Constant definitions
 */

#define BUFSIZE 256

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
  E_UNSUPPORTED_PLATFORM =  13,  /** Unsupported platform. */
  E_INITIALIZATION_ERROR =  14   /** Initialization error. */
} RC;


/**
 *  Constant definitions: libcurl configuration and Skylark status codes.
 */

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
} response_status_t;


// Header definitions
#define STREAM_ENCODING         "Transfer-Encoding: chunked"
#define SBP_V2_ACCEPT_TYPE      "Accept: application/vnd.swiftnav.broker.v1+sbp2"
#define SBP_V2_CONTENT_TYPE     "Content-Type: application/vnd.swiftnav.broker.v1+sbp2"
#define DEVICE_UID_HEADER_FMT   "Device-Uid: %s"
#define DEFAULT_BROKER_ENDPOINT "https://broker.skylark2.swiftnav.com"
#define USER_AGENT              "libskylark-agent/1.0"

/**
 *  Constant definitions: Skylark - Piksi settings groups and boilerplate.
 */

// TODO (mookerji): Finish these
#define SETTINGS_SKYLARK_GROUP  "skylark"
#define SETTINGS_SKYLARK_ENABLE "enable"
#define SETTINGS_SKYLARK_URL    "url"

#define FILE_SKYLARK_UUID       "/cfg/skylark_device_uuid"
#define FILE_SBP_SENDER_ID      "/cfg/sbp_sender_id"
#define UUID4_SIZE              37

/* static const char const * skylark_mode_enum[] = {"ON", "OFF", NULL}; */
/* static struct setting_type skylark_mode_settings_type; */
/* static int TYPE_SKYLARK_MODE = 0; */
/* enum {SKYLARK_ENABLED, SKYLARK_DISABLED}; */

/**
 *  Constant definitions: Common type definitions around configuration and
 *  header files.
 */

typedef size_t (*callback)(char *p, size_t size, size_t n, void *up);

// TODO (mookerji): Document these
// TODO (mookerji): Fixup fd stuff
typedef struct {
  char endpoint_url[BUFSIZE];
  char accept_type_header[BUFSIZE];
  char content_type_header[BUFSIZE];
  char user_agent[BUFSIZE];
  char encoding[BUFSIZE];
  char device_uuid[BUFSIZE];
  char device_header[BUFSIZE];
  u16 sbp_sender_id;
  int fd;
  u8 enabled;
} client_config_t;

void log_client_config(const client_config_t *config);

int client_config_compare(const client_config_t *a, const client_config_t *b);

/**
 *  Settings and HTTP client configuration setup.
 */

RC setup_settings(client_config_t* config);

RC get_device_uuid(char* uuid);
RC get_device_header(const char* uuid, char* uuid_header);
RC get_broker_endpoint(char* endpoint_url);
RC init_config(client_config_t* config);

RC setup_globals(void);
void teardown_globals(void);

/**
 *  Download (i.e., rover) processes.
 */

RC download_process(client_config_t* config, callback cb);

/**
 *  Upload processes, for base stations and reference station processing.
 */

RC upload_process(client_config_t *config, callback cb);

#endif /* SWIFTNAV_LIBSKYLARK_H */
