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

static void http2_setup_download(CURL *curl)
{
  /* HTTP/2 please */ 
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);

  /* we use a self-signed test server, skip verification during debugging */ 
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);

  /* wait for pipe connection to confirm */ 
  curl_easy_setopt(curl, CURLOPT_PIPEWAIT, 1L);
}

static void http2_setup_upload(CURL *curl)
{
  /* upload please */ 
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

  /* HTTP/2 please */ 
  curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);

  /* we use a self-signed test server, skip verification during debugging */ 
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);

  /* wait for pipe connection to confirm */ 
  curl_easy_setopt(curl, CURLOPT_PIPEWAIT, 1L);
}

void skylark_http2(network_context_t *ctx_up, network_context_t *ctx_down)
{
  int still_running = 0; /* keep number of running handles */ 
  network_ctx_setup(ctx_up);
  network_ctx_setup(ctx_down);

  CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
  if (code != CURLE_OK) {
    piksi_log(LOG_ERR, "network_setup global init error %d", code);
    return;
  }

  CURL *curl_up = network_curl_init(ctx_up);

  if (NULL == curl_up) {
    piksi_log(LOG_ERR, "network_curl_init failed");
    curl_global_cleanup();
    return;
  }

  CURL *curl_down = network_curl_init(ctx_down);

  if (NULL == curl_down) {
    piksi_log(LOG_ERR, "network_curl_init failed");
    curl_global_cleanup();
    return;
  }

  CURLM *multi_handle = curl_multi_init();

  struct curl_slist *chunk_down = skylark_init(curl_down);
  chunk_down = curl_slist_append(chunk_down, "Accept: application/vnd.swiftnav.broker.v1+sbp2");
  network_setup_download(chunk_down, ctx_down, curl_down);
  http2_setup_download(curl_down);

  struct curl_slist *chunk_up = skylark_init(curl_up);
  chunk_up = curl_slist_append(chunk_up, "Transfer-Encoding: chunked");
  chunk_up = curl_slist_append(chunk_up, "Content-Type: application/vnd.swiftnav.broker.v1+sbp2");
  skylark_setup_upload(chunk_up, ctx_up, curl_up);
  http2_setup_upload(curl_up);

  curl_multi_add_handle(multi_handle, curl_down);
  curl_multi_add_handle(multi_handle, curl_up);

  curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);

  /* We do HTTP/2 so let's stick to one connection per host */ 
  curl_multi_setopt(multi_handle, CURLMOPT_MAX_HOST_CONNECTIONS, 1L);

  /* we start some action by calling perform right away */ 
  curl_multi_perform(multi_handle, &still_running);

  while(still_running) {
    struct timeval timeout;
    int rc; /* select() return code */ 
    CURLMcode mc; /* curl_multi_fdset() return code */ 

    fd_set fdread;
    fd_set fdwrite;
    fd_set fdexcep;
    int maxfd = -1;

    long curl_timeo = -1;

    FD_ZERO(&fdread);
    FD_ZERO(&fdwrite);
    FD_ZERO(&fdexcep);

    /* set a suitable timeout to play around with */ 
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    curl_multi_timeout(multi_handle, &curl_timeo);
    if(curl_timeo >= 0) {
      timeout.tv_sec = curl_timeo / 1000;
      if(timeout.tv_sec > 1)
        timeout.tv_sec = 1;
      else
        timeout.tv_usec = (curl_timeo % 1000) * 1000;
    }

    /* get file descriptors from the transfers */ 
    mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

    if(mc != CURLM_OK) {
      fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
      break;
    }

    /* On success the value of maxfd is guaranteed to be >= -1. We call
       select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
       no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
       to sleep 100ms, which is the minimum suggested value in the
       curl_multi_fdset() doc. */ 

    if(maxfd == -1) {
      struct timeval wait = { 0, 100 * 1000 }; /* 100ms */ 
      rc = select(0, NULL, NULL, NULL, &wait);
    }
    else {
      /* Note that on some platforms 'timeout' may be modified by select().
         If you need access to the original value save a copy beforehand. */ 
      rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
    }

    switch(rc) {
    case -1:
      /* select error */ 
      break;
    case 0:
    default:
      /* timeout or readable/writable sockets */ 
      curl_multi_perform(multi_handle, &still_running);
      break;
    }
  }

  curl_multi_cleanup(multi_handle);

  curl_slist_free_all(chunk_down);
  curl_slist_free_all(chunk_up);

  curl_easy_cleanup(curl_down);
  curl_easy_cleanup(curl_up);
}
