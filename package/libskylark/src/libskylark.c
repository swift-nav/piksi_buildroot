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

#define VERBOSE

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

// TODO (mookerji): Repeated here as is everwhere else! This needs to be moved
// into a piksi buildroot library.
static RC get_sbp_sender_id(uint16_t *sender_id)
{
  *sender_id = DEFAULT_SBP_SENDER_ID;
  char sbp_sender_id_string[32];
  if (file_read_string(FILE_SBP_SENDER_ID, sbp_sender_id_string,
                       sizeof(sbp_sender_id_string)) < 0) {
    return E_GENERIC_ERROR;
  }
  *sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  return NO_ERROR;
}

/* const char* client_strerror(RC code) */
/* { */
/*   switch (code) { */
/*   case NO_ERROR: */
/*     return "No error"; */
/*   case E_NOT_IMPLEMENTED: */
/*     return "Not implemented"; */
/*   case E_GENERIC_ERROR: */
/*     return "Generic error"; */
/*   case E_NULL_VALUE_ERROR: */
/*     return "Required value was NULL"; */
/*   case E_CONF_READ_ERROR: */
/*     return "Client: Error reading client configuration parameters"; */
/*   case E_INITIALIZATION_ERROR: */
/*     return "Client: Initialization error"; */
/*   case E_NETWORK_UNAVAILABLE: */
/*     return "Client: The network is unavailable"; */
/*   case E_CONNECTION_LOST: */
/*     return "Client: The network connection was lost"; */
/*   case E_RECONNECTION_FAILED: */
/*     return "Client: Network reconnection failed"; */
/*   case E_SUB_CONNECTION_ERROR: */
/*     return "Client: Subscribe connection failed"; */
/*   case E_SUB_WRITE_ERROR: */
/*     return "Client: Error writing SBP to Piksi"; */
/*   case E_PUB_CONNECTION_ERROR: */
/*     return "Client: Publish connection failed"; */
/*   case E_PUB_READ_ERROR: */
/*     return "Client: Error reading SBP from Piksi"; */
/*   case E_BAD_HTTP_HEADER: */
/*     return "Server: Bad HTTP header"; */
/*   case E_NO_ROVER_POS_FOUND: */
/*     return "Server: Required rover position was not found"; */
/*   case E_MAX_ERROR: */
/*   default: { */
/*     return "Client: UNHANDLED ERROR"; */
/*   } */
/* } */

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
  log_debug("client_config: sbp_sender_id=%d\n", config->sbp_sender_id);
  log_debug("client_config: enabled=%d\n", config->enabled);
  log_debug("client_config: fd=%d\n", config->fd);
}

/**
 *  Settings and HTTP client configuration setup.
 */

RC setup_settings(client_config_t *config)
{
  (void)config;
  return NO_ERROR;
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

RC get_broker_endpoint(char *endpoint_url)
{
  (void)strcpy(endpoint_url, DEFAULT_BROKER_ENDPOINT);
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
  if ((rc = get_broker_endpoint(config->endpoint_url)) < NO_ERROR) {
    return rc;
  }
  if ((rc = get_sbp_sender_id(&(config->sbp_sender_id))) < NO_ERROR) {
    return rc;
  }
  strcpy(config->accept_type_header, SBP_V2_ACCEPT_TYPE);
  strcpy(config->content_type_header, SBP_V2_CONTENT_TYPE);
  strcpy(config->encoding, STREAM_ENCODING);
  strcpy(config->user_agent, USER_AGENT);
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
void teardown_globals(void) { curl_global_cleanup(); }
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
size_t download_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  int *fd = (int *)userdata;
  ssize_t m = write(*fd, ptr, size * nmemb);
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
 * \param config Skylark Client configuration
 * \param cb     Callback function writing to a file descriptor.
 * \return  RC return code indicating success or failure
 */
RC download_process(client_config_t *config, write_callback_fn cb)
{
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    return -E_NULL_VALUE_ERROR;
  }
  curl_easy_setopt(curl, CURLOPT_URL, config->endpoint_url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &config->fd);
#ifdef VERBOSE
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif
  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, STREAM_ENCODING);
  chunk = curl_slist_append(chunk, SBP_V2_ACCEPT_TYPE);
  chunk = curl_slist_append(chunk, config->device_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    log_error("%d %s\n", res, curl_easy_strerror(res));
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
size_t upload_callback(void *buffer, size_t size, size_t nitems, void *instream)
{
  int *fd = (int *)instream;
  ssize_t m = read(*fd, buffer, size * nitems);
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
 * \param config Skylark Client configuration
 * \param cb     Callback function writing to a file descriptor.
 * \return  RC return code indicating success or failure
 */
RC upload_process(client_config_t *config, read_callback_fn cb)
{
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    return -E_NULL_VALUE_ERROR;
  }
  curl_easy_setopt(curl, CURLOPT_URL, config->endpoint_url);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, cb);
  curl_easy_setopt(curl, CURLOPT_READDATA, &config->fd);
#ifdef VERBOSE
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif
  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, STREAM_ENCODING);
  chunk = curl_slist_append(chunk, SBP_V2_CONTENT_TYPE);
  chunk = curl_slist_append(chunk, config->device_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  CURLcode res = curl_easy_perform(curl);
  log_info("upload_process after perform\n");
  // TODO (mookerji): Consider having retries and stuff around here.
  // TODO (mookerji): Signal handling?
  if (res != CURLE_OK) {
    log_error("%d %s\n", res, curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return -E_PUB_CONNECTION_ERROR;
  }
  curl_easy_cleanup(curl);
  return NO_ERROR;
}
