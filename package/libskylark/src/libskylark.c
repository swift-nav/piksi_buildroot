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

#include <curl/curl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libskylark.h"

/**
 * Functions and utilities related to interfacing with Swift Skylark service.
 *
 * This library defines a suite of shared functionality required for all clients
 * to Skylark, containing some utility functions around reading, managing, and
 * displaying client configurations, as well as libcurl-based requests for SBP
 * publish and subscribe of SBP data over HTTP.
 */

/**
 *  Utilities
 */

// TODO (mookerji): Repeated here as is everywhere else! This needs to be moved
// into a piksi buildroot library.
static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    log_error("error opening %s\n", filename);
    return -1;
  }
  bool success = (fgets(str, str_size, fp) != NULL);
  fclose(fp);
  if (!success) {
    log_error("error reading %s\n", filename);
    return -1;
  }
  return 0;
}

/** Serialize client return code to a human readable message.
 *
 * \param code  Client return code
 * \return  Error message
 */
const char *client_strerror(RC code)
{
  switch (code) {
    case NO_ERROR:
      return "No error";
    case E_NOT_IMPLEMENTED:
      return "Not implemented";
    case E_GENERIC_ERROR:
      return "Generic error";
    case E_NULL_VALUE_ERROR:
      return "Required value was NULL";
    case E_CONF_READ_ERROR:
      return "Client: Error reading client configuration parameters";
    case E_INITIALIZATION_ERROR:
      return "Client: Initialization error";
    case E_NETWORK_UNAVAILABLE:
      return "Client: The network is unavailable";
    case E_CONNECTION_LOST:
      return "Client: The network connection was lost";
    case E_RECONNECTION_FAILED:
      return "Client: Network reconnection failed";
    case E_SUB_CONNECTION_ERROR:
      return "Client: Subscribe connection failed";
    case E_SUB_WRITE_ERROR:
      return "Client: Error writing SBP to Piksi";
    case E_PUB_CONNECTION_ERROR:
      return "Client: Publish connection failed";
    case E_PUB_READ_ERROR:
      return "Client: Error reading SBP from Piksi";
    case E_BAD_HTTP_REQUEST:
      return "Server: Bad HTTP header or request";
    case E_NO_ROVER_POS_FOUND:
      return "Server: Required rover position was not found";
    case E_UNAUTHORIZED_CLIENT:
      return "Server: Client is not authorized to use this service";
    case E_RETRIES_EXHAUSTED:
      return "Client: Client connection retries exhausted";
    case E_MAX_ERROR:
    default: {
      return "Client: UNHANDLED ERROR";
    }
  }
}

/** Debug log client return code.
 *
 * \param code  Client return code
 */
void log_client_error(RC code)
{
  log_debug("client_debug code=%d msg=%s\n", code, client_strerror(-code));
}

/** Debug log CURL error codes
 *
 * \param code  curl return code
 */
static void log_curl_error(CURLcode code)
{
  log_error("curl_debug code=%d msg=%s\n", code, curl_easy_strerror(code));
}

/**
 *  Configuration
 */

/** Debug log a human-readable representation of the Skylark configuration.
 *
 * \param config  Skylark client configuration.
 * \return  RC
 */
void log_client_config(const client_config_t *config)
{
  log_debug("client_config: endpoint_url=%s\n", config->endpoint_url);
  log_debug("client_config: device_uuid=%s\n", config->device_uuid);
  log_debug("client_config: fd=%d\n", config->fd);
  log_debug("client_config: num_retries=%d\n", config->num_retries);
  log_debug("client_config: retry_delay=%d\n", config->retry_delay);
  log_debug("client_config: retry_max_time=%d\n", config->retry_max_time);
}

/** Read the Skylark device UUID.
 *
 * \param uuid  UUID4 string to return.
 * \return  RC return code indicating success or failure
 */
RC get_device_uuid(char *uuid)
{
  if (file_read_string(FILE_SKYLARK_UUID, uuid, UUID4_SIZE) < 0) {
    return E_GENERIC_ERROR;
  }
  return NO_ERROR;
}

/** Format a device UUID (UUID4) for an HTTP request.
 *
 * \param uuid         Device UUID4
 * \param uuid_header  Formatted string header to return.
 * \return  RC return code indicating success or failure
 */
RC get_device_header(const char *uuid, char *uuid_header)
{
  // TODO: Handle error code here
  sprintf(uuid_header, DEVICE_UID_HEADER_FMT, uuid);
  return NO_ERROR;
}

/** Initialize a Skylark client configuration.
 *
 * \param config Client configuration to initialize
 * \return  RC return code indicating success or failure
 */
RC init_config(client_config_t *config)
{
  RC rc = NO_ERROR;
  if ((rc = get_device_uuid(config->device_uuid)) < NO_ERROR) {
    return rc;
  }
  if ((rc = get_device_header(config->device_uuid, config->device_header)) <
      NO_ERROR) {
    return rc;
  }
  config->num_retries = 0;
  config->retry_delay = 0;
  config->retry_max_time = 0;
  config->fd = 0;
  memset(config->endpoint_url, '\0', BUFSIZE);
  return rc;
}

/** Setup libcurl global variables.
 *
 * \return  RC return code indicating success or failure
 */
RC setup_globals(void)
{
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    log_error("%d %s\n", res, curl_easy_strerror(res));
    return -E_INITIALIZATION_ERROR;
  }
  return NO_ERROR;
}

/** Cleanup libcurl global variables.
 *
 * \return  RC return code indicating success or failure
 */
void teardown_globals(void) {
  curl_global_cleanup();
}

/**
 *  Download (i.e., rover) processes.
 */

/** Callback for the download process: writes to a file descriptor.
 *
 * For more details, see:
 * https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
 *
 * \param ptr       Pointer to the data received by libcurl
 * \param size      Item size
 * \param nmemb     Number of items
 * \param userdata  Pointer to destination
 * \return RC return code indicating success or failure
 */
static size_t download_callback(char *ptr, size_t size, size_t nmemb,
                                void *userdata)
{
  int *fd = (int *)userdata;
  ssize_t m = write(*fd, ptr, size * nmemb);
#ifdef VERBOSE
  log_error("download_callback! size=%d items=%d\n", (int)size, (int)nmemb);
#endif
  return m;
}

/** libcurl-based subscribe request; streams SBP over a continuous connection
 * and writes that data to a file descriptor passed in the client
 * configuration. The expectation here is that a reader of that file descriptor
 * is passing it on to the firmware. The write to the file descriptor is handled
 * via the passed callback function.
 *
 * Dataflow:
 *   HTTP GET => callback writer => config->fd => reader in external process
 *
 * \param config   Skylark Client configuration
 * \param verbose  Verbose output
 * \return  RC return code indicating success or failure
 */
RC download_process(client_config_t *config, bool verbose)
{
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    return -E_NULL_VALUE_ERROR;
  }
  curl_easy_setopt(curl, CURLOPT_URL, config->endpoint_url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &config->fd);
  if (verbose) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }
  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, STREAM_ENCODING);
  chunk = curl_slist_append(chunk, SBP_V2_ACCEPT_TYPE);
  chunk = curl_slist_append(chunk, config->device_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    log_curl_error(res);
    curl_easy_cleanup(curl);
    return -E_SUB_CONNECTION_ERROR;
  }
  curl_easy_cleanup(curl);
  return NO_ERROR;
}

/** Callback for the upload process: reads from a file descriptor.
 *
 * For more details, see:
 * https://curl.haxx.se/libcurl/c/CURLOPT_READFUNCTION.html
 *
 * \param buffer    Pointer to libcurl buffer
 * \param size      Size of items
 * \param nitems    Number of items
 * \param instream  Pointer to file descriptor to read from
 * \return RC return code indicating success or failure
 */
static size_t upload_callback(char *buffer, size_t size, size_t nitems,
                              void *instream)
{
  int *fd = (int *)instream;
  ssize_t m = read(*fd, buffer, size * nitems);
#ifdef VERBOSE
  log_error("upload_callback! size=%d items=%d\n", (int)size, (int)nitems);
#endif
  if (m < 0) {
    return CURL_READFUNC_ABORT;
  }
  return m;
}

/** libcurl-based publish request; streams SBP through a continuous connection,
 * reading that data from a file descriptor passed in the client
 * configuration. The expectation here is that another process is writing to
 * that file descriptor, passing data from the firmware. The read from the
 * descriptor is handled via the passed callback function.
 *
 * Dataflow:
 *   writer in external process => callback reader => HTTP PUT
 *
 * \param config   Skylark Client configuration
 * \param verbose  Verbose output
 * \return  RC return code indicating success or failure
 */
RC upload_process(client_config_t *config, bool verbose)
{
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    return -E_NULL_VALUE_ERROR;
  }
  curl_easy_setopt(curl, CURLOPT_URL, config->endpoint_url);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, upload_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &config->fd);
  if (verbose) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  }
  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, STREAM_ENCODING);
  chunk = curl_slist_append(chunk, SBP_V2_CONTENT_TYPE);
  chunk = curl_slist_append(chunk, config->device_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  // Setup some state for checking retries. num_retries counts down to zero and
  // retry_time_elapsed keeps track of the total amount of time (in seconds)
  // that we've been retrying for. If we exceed exhaust our number of retries or
  // exceed the total time we can retry for (config->retry_max_time), then bail.
  int num_retries = config->num_retries;
  long retry_time_elapsed = 0;
  RC ret = NO_ERROR;
  // Loop until we've exceeded our retries.
  for (;;) {
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      log_curl_error(res);
      ret = E_PUB_CONNECTION_ERROR;
    }
    // Update retry state, and break out if necessary. The error code returned
    // from this If config->num_retries is non-zero, we assume that the
    // application user intended to retry, and log an error message.
    num_retries--;
    retry_time_elapsed += config->retry_delay;
    if (num_retries <= 0 || retry_time_elapsed >= config->retry_max_time) {
      if (config->num_retries > 0) {
        log_client_error(E_RETRIES_EXHAUSTED);
      }
      break;
    }
    usleep(config->retry_delay * USEC_IN_SEC);
    long response;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
    switch (response) {
      case STATUS_PATH_NOT_FOUND:
      case STATUS_BAD_HTTP_REQUEST: {
        ret = E_BAD_HTTP_REQUEST;
        break;
      }
      case STATUS_UNAUTHORIZED_DEVICE: {
        ret = E_UNAUTHORIZED_CLIENT;
        break;
      }
      // response=0 when res != CURLE_OK.
      case 0: {
        break;
      }
      case STATUS_PUT_OK:
      default: {
        log_debug("Client: Unhandled error for publishing\n");
        ret = E_GENERIC_ERROR;
        break;
      }
    }
  }
  curl_easy_cleanup(curl);
  return ret;
}
