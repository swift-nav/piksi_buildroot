/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

/* somewhat unix-specific */ 
#include <sys/time.h>
#include <unistd.h>

/* curl stuff */ 
#include <curl/curl.h>

#include <libpiksi/logging.h>

#include <libnetwork/libnetwork.h>
#include <libnetwork/libnetwork_http2.h>

#define TIMEOUT_MAX_S 1

static
void dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size)
{
  size_t i;
  size_t c;
  unsigned int width=0x10;
 
  fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
          text, (long)size, (long)size);
 
  for(i=0; i<size; i+= width) {
    fprintf(stream, "%4.4lx: ", (long)i);
 
    /* show hex to the left */
    for(c = 0; c < width; c++) {
      if(i+c < size)
        fprintf(stream, "%02x ", ptr[i+c]);
      else
        fputs("   ", stream);
    }
 
    /* show data on the right */
    for(c = 0; (c < width) && (i+c < size); c++) {
      char x = (ptr[i+c] >= 0x20 && ptr[i+c] < 0x80) ? ptr[i+c] : '.';
      fputc(x, stream);
    }
 
    fputc('\n', stream); /* newline */
  }
}
 
static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp)
{
  const char *text;
  (void)handle; /* prevent compiler warning */
  (void)userp;
 
  switch (type) {
  case CURLINFO_END:
  case CURLINFO_TEXT:
    fprintf(stderr, "== Info: %s", data);
  default: /* in case a new one is introduced to shock us */
    return 0;
 
  case CURLINFO_HEADER_OUT:
    text = "=> Send header";
    break;
  case CURLINFO_DATA_OUT:
    text = "=> Send data";
    break;
  case CURLINFO_SSL_DATA_OUT:
    text = "=> Send SSL data";
    break;
  case CURLINFO_HEADER_IN:
    text = "<= Recv header";
    break;
  case CURLINFO_DATA_IN:
    text = "<= Recv data";
    break;
  case CURLINFO_SSL_DATA_IN:
    text = "<= Recv SSL data";
    break;
  }
 
  dump(text, stderr, (unsigned char *)data, size);
  return 0;
}

static void http2_setup(CURL *curl)
{
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);

  /* we use a self-signed test server, skip verification during debugging */ 
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);

  curl_easy_setopt(curl, CURLOPT_PIPEWAIT, 1L);
}

static int http2_setup_download(network_context_t *ctx_down, CURL **curl, struct curl_slist **chunk_down)
{
  network_ctx_setup(ctx_down);

  *curl = network_curl_init(ctx_down);
  if (NULL == curl) {
    piksi_log(LOG_ERR, "network_curl_init failed");
    return 1;
  }

  *chunk_down = skylark_init(*curl);
  *chunk_down = curl_slist_append(*chunk_down, "Accept: application/vnd.swiftnav.broker.v1+sbp2");

  network_setup_download(ctx_down, *chunk_down);

  http2_setup(*curl);

  return 0;
}

static int http2_setup_upload(network_context_t *ctx_up, CURL **curl, struct curl_slist **chunk_up)
{
  network_ctx_setup(ctx_up);

  *curl = network_curl_init(ctx_up);
  if (NULL == curl) {
    piksi_log(LOG_ERR, "network_curl_init failed");
    return 1;
  }

  *chunk_up = skylark_init(*curl);
  *chunk_up = curl_slist_append(*chunk_up, "Transfer-Encoding: chunked");
  *chunk_up = curl_slist_append(*chunk_up, "Content-Type: application/vnd.swiftnav.broker.v1+sbp2");
  skylark_setup_upload(ctx_up, *chunk_up);

  curl_easy_setopt(*curl, CURLOPT_UPLOAD, 1L);

  http2_setup(*curl);

  return 0;
}

static int http2_setup_multi(CURL *curl_down, CURL *curl_up, CURLM **multi)
{
  *multi = curl_multi_init();

  curl_multi_add_handle(*multi, curl_down);
  curl_multi_add_handle(*multi, curl_up);

  curl_multi_setopt(*multi, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);

  curl_multi_setopt(*multi, CURLMOPT_MAX_HOST_CONNECTIONS, 1L);

  return 0;
}

static void http2_cleanup(CURLM *multi, struct curl_slist *chunk_down, struct curl_slist *chunk_up)
{
  if (multi) {
    curl_multi_cleanup(multi);
  }

  if (chunk_down) {
    curl_slist_free_all(chunk_down);
  }

  if (chunk_up) {
    curl_slist_free_all(chunk_up);
  }

  curl_global_cleanup();
}

static void http2_timeout(CURLM *multi, struct timeval *timeout)
{
  long curl_timeo_ms = -1;

  curl_multi_timeout(multi, &curl_timeo_ms);
  if(curl_timeo_ms >= 0 && curl_timeo_ms <  TIMEOUT_MAX_S * 1000) {
    *timeout = (struct timeval){ .tv_sec = 0, .tv_usec = curl_timeo_ms * 1000 };
  } else {
    /* Max timeout */
    *timeout = (struct timeval){ .tv_sec = TIMEOUT_MAX_S, .tv_usec = 0 };
  }
}

static void http2_loop(CURLM *multi)
{
  int active = 0;
  curl_multi_perform(multi, &active);

  while (active) {
    struct timeval timeout = {0};
    int rc, maxfd = -1;

    fd_set fdread, fdwrite, fdexcep;
    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    CURLMcode mc = curl_multi_fdset(multi, &fdread, &fdwrite, &fdexcep, &maxfd);

    if (mc != CURLM_OK) {
      piksi_log(LOG_ERR, "curl_multi_fdset() failed, code %d", mc);
      break;
    }

    if (maxfd == -1) {
      /* https://curl.haxx.se/libcurl/c/curl_multi_fdset.html
       * Recommended wait is 100 ms in this case.
       */
      timeout = (struct timeval){ 0, 100 * 1000 };
      rc = select(0, NULL, NULL, NULL, &timeout);
    } else {
      http2_timeout(multi, &timeout);
      rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
    }

    if (rc >= 0) {
      curl_multi_perform(multi, &active);
    }
  }
}

void skylark_http2(network_context_t *ctx_up, network_context_t *ctx_down)
{
  assert(ctx_up);
  assert(ctx_down);

  CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
  if (code != CURLE_OK) {
    piksi_log(LOG_ERR, "network_setup global init error %d", code);
    return;
  }

  CURL *curl_down = NULL;
  struct curl_slist *chunk_down = NULL;
  if (http2_setup_download(ctx_down, &curl_down, &chunk_down)) {
    goto cleanup;
  }

  CURL *curl_up = NULL;
  struct curl_slist *chunk_up = NULL;
  if (http2_setup_upload(ctx_up, &curl_up, &chunk_up)) {
    goto cleanup;
  }

  CURLM *multi = NULL;
  if (http2_setup_multi(curl_down, curl_up, &multi)) {
    goto cleanup;
  }

  http2_loop(multi);

cleanup:
  http2_cleanup(multi, chunk_down, chunk_up);
}
