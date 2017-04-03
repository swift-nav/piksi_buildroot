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

#ifndef SWIFTNAV_LIBSKYLARK_H
#define SWIFTNAV_LIBSKYLARK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

/** Maximum buffer size */
#define BUFSIZE 256

/** Return codes. */
typedef enum {
  NO_ERROR = 0,
  E_NOT_IMPLEMENTED = 1,
  E_GENERIC_ERROR = 2,
  E_NULL_VALUE_ERROR = 3,
  E_CONF_READ_ERROR = 4,
  E_INITIALIZATION_ERROR = 5,
  E_NETWORK_UNAVAILABLE = 6,
  E_CONNECTION_LOST = 7,
  E_RECONNECTION_FAILED = 8,
  E_SUB_CONNECTION_ERROR = 9,
  E_SUB_WRITE_ERROR = 10,
  E_PUB_CONNECTION_ERROR = 11,
  E_PUB_READ_ERROR = 12,
  E_BAD_HTTP_HEADER = 13,
  E_NO_ROVER_POS_FOUND = 14,
  E_UNAUTHORIZED_CLIENT = 15,
  E_MAX_ERROR = 16,
} RC;

const char *client_strerror(RC code);
void log_client_error(RC code);

/**
 *  Constant definitions: libcurl configuration and Skylark status codes.
 */

/** Defined Skylark HTTP codes. */
typedef enum {
  STATUS_PUT_OK = 200,
  STATUS_GET_OK = 202,
  STATUS_NO_HEADER = 400,
  STATUS_BAD_HEADER = 400,
  STATUS_NO_ROVER_POSITION = 404,
  STATUS_SOURCE_EXIT = 400,
  STATUS_BASE_EXIT = 500,
  STATUS_PATH_NOT_FOUND = 404,
  STATUS_METHOD_NOT_FOUND = 400,
} response_status_t;

// Header definitions
#define STREAM_ENCODING "Transfer-Encoding: chunked"
#define SBP_V2_ACCEPT_TYPE "Accept: application/vnd.swiftnav.broker.v1+sbp2"
#define SBP_V2_CONTENT_TYPE \
  "Content-Type: application/vnd.swiftnav.broker.v1+sbp2"
#define DEVICE_UID_HEADER_FMT "Device-Uid: %s"
#define USER_AGENT "libskylark-agent/1.0"

/**
 *  Constant definitions: Skylark - Piksi settings groups and boilerplate.
 */

#define SETTINGS_SKYLARK_GROUP "skylark"
#define SETTINGS_SKYLARK_ENABLE "enable"
#define SETTINGS_SKYLARK_URL "url"

#define FILE_SKYLARK_UUID "/cfg/device_uuid"
#define UUID4_SIZE 37

/**
 *  Constant definitions: Common type definitions around configuration and
 *  header files.
 */

/** Callback functions for reading/writing pipe file descriptors. */
typedef size_t write_callback_fn(char *ptr, size_t size, size_t nmemb,
                                 void *userdata);
typedef size_t read_callback_fn(char *buffer, size_t size, size_t nitems,
                                void *instream);

/** Structure containing Skylark client configuration. */
typedef struct {
  char endpoint_url[BUFSIZE]; /**< Request endpoint */
  char device_uuid[BUFSIZE]; /**< Device UUID (UUID4) */
  char device_header[BUFSIZE];
  int num_retries;    /**< Number of retries */
  int retry_delay;    /**< Delay between retries (seconds) */
  int retry_max_time; /**< Maximum time to keep retrying (seconds) */
  int fd;                 /**< Pipe file descriptor */
} client_config_t;

void log_client_config(const client_config_t *config);

int client_config_compare(const client_config_t *a, const client_config_t *b);

/**
 *  Settings and HTTP client configuration setup.
 */

RC get_device_uuid(char *uuid);
RC get_device_header(const char *uuid, char *uuid_header);
RC init_config(client_config_t *config);

RC setup_globals(void);
void teardown_globals(void);

/**
 *  Download (i.e., rover) processes.
 */

size_t download_callback(char *ptr, size_t size, size_t nmemb, void *userdata);

RC download_process(client_config_t *config, write_callback_fn cb,
                    bool verbose);

/**
 *  Upload processes, for base stations and reference station processing.
 */

size_t upload_callback(char *buffer, size_t size, size_t nitems,
                       void *instream);

RC upload_process(client_config_t *config, read_callback_fn cb, bool verbose);

#endif /* SWIFTNAV_LIBSKYLARK_H */
