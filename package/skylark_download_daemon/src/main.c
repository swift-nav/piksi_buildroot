/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Mark Fine <mark@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sbp_zmq.h>

static size_t callback(void *p, size_t size, size_t n, void *up)
{
  ssize_t m = write((int)up, p, size*n);
  return m;
}

static void download(int fd)
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

  curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:3000");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);

  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk, "Accept: application/vnd.swiftnav.broker.v1+sbp2");
  chunk = curl_slist_append(chunk, "Device-Uid: 22222222-2222-2222-2222-222222222222");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    exit(EXIT_FAILURE);
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

static void sink(int fd)
{
  for (;;) {
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    ssize_t m = fwrite(buf, 1, n, stdout);
    if (m < n) {
      break;
    }
    fflush(stdout);
  }
}

int main(int argc, char *argv[])
{
  int fds[2], ret = pipe(fds);
  if (ret < 0) {
    exit(EXIT_FAILURE);
  }

  ret = fork();
  if (ret < 0) {
    exit(EXIT_FAILURE);
  } else if (ret == 0) {
    close(fds[0]);
    download(fds[1]);
  } else {
    close(fds[1]);
    sink(fds[0]);
    waitpid(ret, &ret, 0);
  }

  exit(EXIT_SUCCESS);
}
