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

#include <curl/curl.h>
#include <libsbp/observation.h>
#include <libsbp/navigation.h>
#include <string.h>
#include <stdio.h>
//#include <sbp_settings.h>
#include <sbp_zmq.h>

#include "libskylark.h"

#define VERBOSE

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
static RC get_sbp_sender_id(u16* sender_id)
{
  *sender_id = SBP_SENDER_ID;
  char sbp_sender_id_string[32];
  if (file_read_string(FILE_SBP_SENDER_ID, sbp_sender_id_string,
                       sizeof(sbp_sender_id_string)) < 0) {
    return E_GENERIC_ERROR;

  }
  *sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  return NO_ERROR;
}

/**
 *  Configuration
 */

void log_client_config(const client_config_t *config)
{
  log_debug("client_config: endpoint_url=%s\n", config->endpoint_url);
  log_debug("client_config: accept_type_header=%s\n", config->accept_type_header);
  log_debug("client_config: content_type_header=%s\n", config->content_type_header);
  log_debug("client_config: encoding=%d\n", config->encoding);
  log_debug("client_config: device_header=%s\n", config->device_header);
  log_debug("client_config: sbp_sender_id=%d\n", config->sbp_sender_id);
  log_debug("client_config: enabled=%d\n", config->enabled);
  log_debug("client_config: fd=%d\n", config->fd);
}

int client_config_compare(const client_config_t *a,
                          const client_config_t *b)
{
  return strcmp(a->endpoint_url, b->endpoint_url) == 0
    && strcmp(a->accept_type_header, b->accept_type_header) == 0
    && strcmp(a->content_type_header, b->content_type_header) == 0
    && strcmp(a->encoding, b->encoding) == 0
    && strcmp(a->device_uuid, b->device_header) == 0
    && (a->sbp_sender_id == b->sbp_sender_id)
    && (a->fd == b->fd)
    && (a->enabled == b->enabled);
}

/**
 *  Settings and HTTP client configuration setup.
 */

// TODO (mookerji): Implementing settings!
RC setup_settings(client_config_t* config)
{
  /* TYPE_SKYLARK_MODE \ */
  /*   = settings_type_register_enum(skylark_mode_enum, &skylark_mode_settings_type); */
  /* READ_ONLY_PARAMETER('skylark', */
  /*                     'enable', */
  /*                     config->enabled, */
  /*                     TYPE_SKYLARK_MODE); */
  /* READ_ONLY_PARAMETER(SETTINGS_SKYLARK_GROUP, */
  /*                     SETTINGS_SKYLARK_URL, */
  /*                     config->endpoint_url, */
  /*                     TYPE_STRING); */
  return NO_ERROR;
}

RC get_device_uuid(char* uuid)
{
  if (file_read_string(FILE_SKYLARK_UUID, uuid, UUID4_SIZE) < 0) {
    return E_GENERIC_ERROR;
  }
  return NO_ERROR;
}

RC get_device_header(const char* uuid, char* uuid_header)
{
  // TODO: Handle error code here
  sprintf(uuid_header, DEVICE_UID_HEADER_FMT, uuid);
  return NO_ERROR;
}

RC get_broker_endpoint(char* endpoint_url)
{
  (void)strcpy(endpoint_url, DEFAULT_BROKER_ENDPOINT);
  return NO_ERROR;
}

RC init_config(client_config_t* config)
{
  RC rc = NO_ERROR;
  if ((rc = get_device_uuid(config->device_uuid)) < NO_ERROR) {
    return rc;
  }
  if ((rc = get_device_header(config->device_uuid,
                              config->device_header)) < NO_ERROR) {
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

RC setup_globals(void)
{
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    log_error(curl_easy_strerror(res));
    return -E_INITIALIZATION_ERROR;
  }
}

void teardown_globals(void)
{
  curl_global_cleanup();
}

/**
 *  Download (i.e., rover) processes.
 */

RC download_process(client_config_t* config, callback cb) {
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
  curl_easy_setopt(curl, CURLOPT_USERAGENT, config->user_agent);
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, config->encoding);
  chunk = curl_slist_append(chunk, config->accept_type_header);
  chunk = curl_slist_append(chunk, config->device_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  CURLcode res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    log_error("%d %s\n", res, curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return -E_SUBSCRIBE_ERROR;
  }
  curl_easy_cleanup(curl);
  return NO_ERROR;
}

/**
 *  Upload processes, for base stations and reference station processing.
 */


RC upload_process(client_config_t *config, callback cb)
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
  curl_easy_setopt(curl, CURLOPT_USERAGENT, config->user_agent);
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, config->encoding);
  chunk = curl_slist_append(chunk, config->content_type_header);
  chunk = curl_slist_append(chunk, config->device_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  CURLcode res = curl_easy_perform(curl);
  log_info("upload_process after perform\n");
  // TODO (mookerji): Consider having retries and stuff around here.
  // TODO (mookerji): Signal handling?
  if (res != CURLE_OK) {
    log_error("%d %s\n", res, curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    return -E_PUBLISH_ERROR;
  }
  curl_easy_cleanup(curl);
  return NO_ERROR;
}
