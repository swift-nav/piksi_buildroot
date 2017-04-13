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

#include <unistd.h>
#include <curl/curl.h>
#include <libpiksi/logging.h>

#include "libskylark.h"

#define AGENT_TYPE   "libskylark-agent/1.0"
#define ACCEPT_TYPE  "application/vnd.swiftnav.broker.v1+sbp2"
#define CONTENT_TYPE "application/vnd.swiftnav.broker.v1+sbp2"

static int skylark_request(const skylark_config_t * const config, CURL *curl)
{
  char error_buf[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buf);

  while (true) {
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      if (config->debug) {
        piksi_log(LOG_DEBUG, "curl error (%d) \"%s\"\n", res, error_buf);
      }
    }

    usleep(1000000);
  }

  return 0;
}

static size_t skylark_download_callback(char *buf, size_t size, size_t n, void *data)
{
  const skylark_config_t * const config = data;

  ssize_t ret = write(config->fd, buf, size * n);

  if (config->debug) {
    piksi_log(LOG_DEBUG, "write bytes (%d) %d\n", size * n, ret);
  }

  return ret;
}

static size_t skylark_upload_callback(char *buf, size_t size, size_t n, void *data)
{
  const skylark_config_t * const config = data;

  ssize_t ret = read(config->fd, buf, size * n);

  if (config->debug) {
    piksi_log(LOG_DEBUG, "read bytes %d\n", ret);
  }

  if (ret < 0) {
    return CURL_READFUNC_ABORT;
  }

  return ret;
}

int skylark_setup(void)
{
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    return -1;
  }

  return 0;
}

void skylark_teardown(void)
{
  curl_global_cleanup();
}

int skylark_download(const skylark_config_t * const config)
{
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    return -1;
  }

  char uuid_buf[256];
  snprintf(uuid_buf, sizeof(uuid_buf), "Device-Uid: %s", config->uuid);

  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk, "Accept: " ACCEPT_TYPE);
  chunk = curl_slist_append(chunk, uuid_buf);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,     AGENT_TYPE);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, skylark_download_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,     config);
  curl_easy_setopt(curl, CURLOPT_URL,           config->url);

  int ret = skylark_request(config, curl);
  curl_easy_cleanup(curl);

  return ret;
}

int skylark_upload(const skylark_config_t * const config)
{
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    return -1;
  }

  char uuid_buf[256];
  snprintf(uuid_buf, sizeof(uuid_buf), "Device-Uid: %s", config->uuid);

  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk, "Content-Type: " CONTENT_TYPE);
  chunk = curl_slist_append(chunk, uuid_buf);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER,   chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,    AGENT_TYPE);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, skylark_upload_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA,     config);
  curl_easy_setopt(curl, CURLOPT_URL,          config->url);
  curl_easy_setopt(curl, CURLOPT_PUT,          1L);

  int ret = skylark_request(config, curl);
  curl_easy_cleanup(curl);

  return ret;
}
