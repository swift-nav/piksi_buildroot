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
#include <sbp_settings.h>
#include <sbp_zmq.h>

#include "libskylark.h"

/**
 *  Utilities
 */

// TODO (mookerji): Repeated here as is everwhere else!
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

void log_client_config(const skylark_client_config_t *config) {
  log_debug("client_config: endpoint_url=%s\n", config->endpoint_url);
  log_debug("client_config: accept_type_header=%s\n", config->accept_type_header);
  log_debug("client_config: content_type_header=%s\n", config->content_type_header);
  log_debug("client_config: encoding=%s\n", config->encoding);
  log_debug("client_config: device_header=%s\n", config->device_header);
  log_debug("client_config: sbp_sender_id=%s\n", config->sbp_sender_id);
  log_debug("client_config: enabled=%d\n", config->enabled);
}

/**
 *  Settings and HTTP client configuration setup.
 */

// TODO (mookerji): Implementing settings!
SKYLARK_RC setup_settings(skylark_client_config_t* config)
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

SKYLARK_RC get_uuid_channel(char* channel_uuid)
{
  (void)channel_uuid;
  return -E_NOT_IMPLEMENTED;
}

SKYLARK_RC get_device_uuid(char* device_uuid)
{
  strcpy(device_uuid, DEFAULT_DEVICE_UID);
  return NO_ERROR;
}

SKYLARK_RC get_broker_endpoint(char* endpoint)
{
  strcpy(endpoint, SBP_V2_ACCEPT_TYPE);
  return NO_ERROR;
}

SKYLARK_RC get_accept_header(char* header)
{
  strcpy(header, SBP_V2_ACCEPT_TYPE);
  return NO_ERROR;
}

SKYLARK_RC get_content_header(char* header)
{
  strcpy(header, SBP_V2_CONTENT_TYPE);
  return NO_ERROR;
}

SKYLARK_RC format_device_header(const char* uuid, char* header)
{
  sprintf(header, DEVICE_UID_HEADER_FMT, uuid);
  return NO_ERROR;
}

SKYLARK_RC get_sbp_sender_id(u16* sender_id)
{
  *sender_id = SBP_SENDER_ID;
  char sbp_sender_id_string[32];
  if (file_read_string("/cfg/sbp_sender_id", sbp_sender_id_string,
                       sizeof(sbp_sender_id_string)) < 0) {
    return E_GENERIC_ERROR;

  }
  *sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  return NO_ERROR;
}

SKYLARK_RC get_config(skylark_client_config_t* config)
{
  SKYLARK_RC rc = NO_ERROR;
  if ((rc = get_broker_endpoint(config->endpoint_url)) < NO_ERROR) {
    return rc;
  }
  log_info("Got broker_endpoint\n");
  if ((rc = get_accept_header(config->accept_type_header)) < NO_ERROR) {
    return rc;
  }
  log_info("Got accept\n");
  if ((rc = get_content_header(config->content_type_header)) < NO_ERROR) {
    return rc;
  }
  log_info("Got encoding\n");
  strcpy(config->encoding, STREAM_ENCODING);
  if ((rc = get_device_uuid(config->device_uuid)) < NO_ERROR) {
    return rc;
  }
  log_info("Got device uuid\n");
  if ((rc = format_device_header(config->device_uuid,
                                 config->device_header)) < NO_ERROR) {
    return rc;
  }
  log_info("Got format device header \n");
  if ((rc = get_sbp_sender_id(&config->sbp_sender_id)) < NO_ERROR) {
    return rc;
  }
  log_info("Got sender_id \n");
  return rc;
}

/**
 *  Download (i.e., rover) processes.
 */

// Callback used by libcurl to pass data read from Skylark. Writes to pipe.
//
// TODO (mookerji): replace
static size_t download_callback(void *p, size_t size, size_t n, void *up)
{
  int *fd = (int *)up;
  ssize_t m = write(*fd, p, size * n);
  return m;
}

SKYLARK_RC download_process(skylark_client_config_t* config) {
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    exit(EXIT_FAILURE);
  }
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    curl_global_cleanup();
    exit(EXIT_FAILURE);
  }
  curl_easy_setopt(curl, CURLOPT_URL, config->endpoint_url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &download_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &config->fd_out);
#ifdef VERBOSE
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif
  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, config->encoding);
  chunk = curl_slist_append(chunk, config->accept_type_header);
  chunk = curl_slist_append(chunk, config->device_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    log_error(curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    exit(EXIT_FAILURE);
  }
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return NO_ERROR;
}

static void base_obs_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  sbp_zmq_state_t *sbp_zmq_state = (sbp_zmq_state_t *)context;
  sbp_zmq_message_send(sbp_zmq_state, SBP_MSG_OBS, len, msg);
}

static void base_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  sbp_zmq_state_t *sbp_zmq_state = (sbp_zmq_state_t *)context;
  sbp_zmq_message_send(sbp_zmq_state, SBP_MSG_BASE_POS_LLH, len, msg);
}

static void base_ecef_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  sbp_zmq_state_t *sbp_zmq_state = (sbp_zmq_state_t *)context;
  sbp_zmq_message_send(sbp_zmq_state, SBP_MSG_BASE_POS_ECEF, len, msg);
}

u32 msg_read(u8 *buf, u32 n, void *context)
{
  int *fd = (int *)context;
  ssize_t m = read(*fd, buf, n);
  return m;
}

SKYLARK_RC download_io_loop(skylark_client_config_t *config)
{
  // TODO sender id is not going to be right here, since we're relaying
  // messages.
  sbp_zmq_state_t sbp_zmq_state;
  if (sbp_zmq_init(&sbp_zmq_state, config->sbp_zmq_config) < 0) {
    exit(EXIT_FAILURE);
  }
  sbp_state_t sbp_state;
  sbp_msg_callbacks_node_t callback_node;
  sbp_state_init(&sbp_state);
  sbp_state_set_io_context(&sbp_state, &config->fd_in);
  sbp_register_callback(&sbp_state, SBP_MSG_OBS,
                        &base_obs_callback, &sbp_zmq_state,
                        &callback_node);
  sbp_msg_callbacks_node_t base_ecef_callback_node;
  sbp_register_callback(&sbp_state, SBP_MSG_BASE_POS_ECEF,
                        &base_ecef_callback, &sbp_zmq_state,
                        &base_ecef_callback_node);
  sbp_msg_callbacks_node_t base_llh_callback_node;
  sbp_register_callback(&sbp_state, SBP_MSG_BASE_POS_LLH,
                        &base_llh_callback, &sbp_zmq_state,
                        &base_llh_callback_node);
  for (;;) {
    sbp_process(&sbp_state, &msg_read);
  }
  sbp_zmq_deinit(&sbp_zmq_state);
  return NO_ERROR;
}

/**
 *  Upload processes, for base stations and reference station processing.
 */

static size_t upload_callback(void *p, size_t size, size_t n, void *up)
{
  int *fd = (int *)up;
  ssize_t m = read(*fd, p, size * n);
  if (m < 0) {
    return CURL_READFUNC_ABORT;
  }
  return m;
}

SKYLARK_RC upload_process(skylark_client_config_t *config)
{
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    exit(EXIT_FAILURE);
  }
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    curl_global_cleanup();
    exit(EXIT_FAILURE);
  }
  curl_easy_setopt(curl, CURLOPT_URL, config->endpoint_url);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, &upload_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &config->fd_in);
#ifdef VERBOSE
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
#endif
  curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, config->encoding);
  chunk = curl_slist_append(chunk, config->content_type_header);
  chunk = curl_slist_append(chunk, config->device_header);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    log_error(curl_easy_strerror(res));
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    exit(EXIT_FAILURE);
  }
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return NO_ERROR;
}

u32 msg_write(u8 *buf, u32 n, void *context)
{
  int *fd = (int *)context;
  ssize_t m = write(*fd, buf, n);
  return m;
}

static void rover_pos_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  int *fd = (int *)context;
  sbp_state_t sbp_state;
  sbp_state_init(&sbp_state);
  sbp_state_set_io_context(&sbp_state, fd);
  sbp_send_message(&sbp_state, SBP_MSG_POS_LLH, sender_id, len, msg,
                   &msg_write);
}

// TODO (mookerji): Handle return codes
// TODO (mookerji): Pass return codes back
SKYLARK_RC upload_io_loop(skylark_client_config_t *config)
{
  sbp_zmq_state_t sbp_zmq_state;
  if (sbp_zmq_init(&sbp_zmq_state, config->sbp_zmq_config) < 0) {
    exit(EXIT_FAILURE);
  }
  sbp_zmq_callback_register(&sbp_zmq_state,
                            SBP_MSG_POS_LLH,
                            &rover_pos_callback,
                            &config->fd_in, NULL);
  sbp_zmq_loop(&sbp_zmq_state);
  sbp_zmq_deinit(&sbp_zmq_state);
  return NO_ERROR;
}
