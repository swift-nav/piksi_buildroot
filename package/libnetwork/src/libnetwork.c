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
#include <libpiksi/util.h>

#include "libnetwork.h"

typedef struct {
  int fd;
  bool debug;
} network_transfer_t;

typedef struct {
  curl_off_t bytes;
  curl_off_t count;
  bool debug;
} network_progress_t;

static size_t network_download_write(char *buf, size_t size, size_t n, void *data)
{
  const network_transfer_t *transfer = data;

  while (true) {
    ssize_t ret = write(transfer->fd, buf, size * n);
    if (ret < 0 && errno == EINTR) {
      continue;
    }

    if (transfer->debug) {
      piksi_log(LOG_DEBUG, "write bytes (%d) %d", size * n, ret);
    }

    return ret;
  }

  return -1;
}

static size_t network_upload_read(char *buf, size_t size, size_t n, void *data)
{
  const network_transfer_t *transfer = data;

  while (true) {
    ssize_t ret = read(transfer->fd, buf, size * n);
    if (ret < 0 && errno == EINTR) {
      continue;
    }

    if (transfer->debug) {
      piksi_log(LOG_DEBUG, "read bytes %d", ret);
    }

    if (ret < 0) {
      return CURL_READFUNC_ABORT;
    }

    return ret;
  }

  return -1;
}

static int network_progress(network_progress_t *progress, curl_off_t bytes)
{
  if (progress->bytes == bytes && progress->count++ > 30) {
    return -1;
  }

  progress->bytes = bytes;
  progress->count = 0;

  return 0;
}

static int network_download_progress(void *data, curl_off_t dltot, curl_off_t dlnow, curl_off_t ultot, curl_off_t ulnow)
{
  (void)dltot;
  (void)ultot;
  (void)ulnow;

  network_progress_t *progress = data;

  if (progress->debug) {
    piksi_log(LOG_DEBUG, "down bytes (%lld) %lld count %lld", dlnow, progress->bytes, progress->count);
  }

  return network_progress(progress, dlnow);
}

static int network_upload_progress(void *data, curl_off_t dltot, curl_off_t dlnow, curl_off_t ultot, curl_off_t ulnow)
{
  (void)dltot;
  (void)dlnow;
  (void)ultot;

  network_progress_t *progress = data;

  if (progress->debug) {
    piksi_log(LOG_DEBUG, "up bytes (%lld) %lld count %lld", ulnow, progress->bytes, progress->count);
  }

  return network_progress(progress, ulnow);
}

static int network_sockopt(void *data, curl_socket_t fd, curlsocktype purpose)
{
  (void)data;
#ifdef TCP_USER_TIMEOUT
  if (purpose == CURLSOCKTYPE_IPCXN) {
    unsigned int timeout = 20000;
    int ret = setsockopt(fd, SOL_TCP, TCP_USER_TIMEOUT, &timeout, sizeof(timeout));
    if (ret < 0) {
      piksi_log(LOG_ERR, "setsockopt error %d", errno);
      return CURL_SOCKOPT_ERROR;
    }
  }
#else
  (void)fd;
  (void)purpose;
#endif
  return CURL_SOCKOPT_OK;
}

static CURL *network_setup(void)
{
  CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
  if (code != CURLE_OK) {
    piksi_log(LOG_ERR, "global init %d", code);
    return NULL;
  }

  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    piksi_log(LOG_ERR, "init");
    curl_global_cleanup();
    return NULL;
  }

  return curl;
}

static void network_teardown(CURL *curl)
{
  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

static void network_request(CURL *curl)
{
  char error_buf[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER,       error_buf);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 15000L);
  curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 0L);
  curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT,     1L);
  curl_easy_setopt(curl, CURLOPT_FORBID_REUSE,      1L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE,     1L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL,     5L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE,      20L);
  curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION,   network_sockopt);

  while (true) {
    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) {
      piksi_log(LOG_ERR, "curl request (%d) \"%s\"", code, error_buf);
    } else {
      long response;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
      piksi_log(LOG_INFO, "curl request code %d", response);
    }

    usleep(1000000);
  }
}

static struct curl_slist *ntrip_init(CURL *curl)
{
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk, "Ntrip-Version: Ntrip/2.0");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,  "NTRIP ntrip-client/1.0");

  return chunk;
}

static struct curl_slist *skylark_init(CURL *curl)
{
  char uuid_buf[256];
  device_uuid_get(uuid_buf, sizeof(uuid_buf));

  char device_buf[256];
  snprintf(device_buf, sizeof(device_buf), "Device-Uid: %s", uuid_buf);

  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, device_buf);
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk, "Accept: application/vnd.swiftnav.broker.v1+sbp2");
  chunk = curl_slist_append(chunk, "Content-Type: application/vnd.swiftnav.broker.v1+sbp2");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,  "skylark-agent/1.0");

  return chunk;
}

void ntrip_download(const network_config_t *config)
{
  network_transfer_t transfer = {
    .fd = config->fd,
    .debug = config->debug,
  };

  network_progress_t progress = {
    .bytes = 0,
    .count = 0,
    .debug = config->debug,
  };

  CURL *curl = network_setup();
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = ntrip_init(curl);

  curl_easy_setopt(curl, CURLOPT_URL,              config->url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    network_download_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,        &transfer);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, network_download_progress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &progress);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

  network_request(curl);

  curl_slist_free_all(chunk);
  network_teardown(curl);
}

void skylark_download(const network_config_t *config)
{
  network_transfer_t transfer = {
    .fd = config->fd,
    .debug = config->debug,
  };

  network_progress_t progress = {
    .bytes = 0,
    .count = 0,
    .debug = config->debug,
  };

  CURL *curl = network_setup();
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = skylark_init(curl);

  curl_easy_setopt(curl, CURLOPT_URL,              config->url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    network_download_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,        &transfer);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, network_download_progress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &progress);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

  network_request(curl);

  curl_slist_free_all(chunk);
  network_teardown(curl);
}

void skylark_upload(const network_config_t *config)
{
  network_transfer_t transfer = {
    .fd = config->fd,
    .debug = config->debug,
  };

  network_progress_t progress = {
    .bytes = 0,
    .count = 0,
    .debug = config->debug,
  };

  CURL *curl = network_setup();
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = skylark_init(curl);

  curl_easy_setopt(curl, CURLOPT_PUT,              1L);
  curl_easy_setopt(curl, CURLOPT_URL,              config->url);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION,     network_upload_read);
  curl_easy_setopt(curl, CURLOPT_READDATA,         &transfer);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, network_upload_progress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &progress);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

  network_request(curl);

  curl_slist_free_all(chunk);
  network_teardown(curl);
}
