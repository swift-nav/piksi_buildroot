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

#include <curl/curl.h>
#include <stdlib.h>
#include <unistd.h>

#include <libskylark.h>

#define VERBOSE

//
// Upload Daemon - connects to Skylark and sends SBP messages.
//

static size_t upload_callback(void *p, size_t size, size_t n, void *up)
{
  int *fd = (int *)up;
  ssize_t m = read(*fd, p, size * n);
  printf("%d\n", m);
  if (m < 0) {
    return CURL_READFUNC_ABORT;
  }
  return m;
}

int main(void)
{
  printf("starting upload daemon\n");
  int fd;
  char * fifo = "/tmp/skylark_upload";
  mkfifo(fifo, 0666);
  fd = open(fifo, O_RDONLY);

  client_config_t config;
  (void)init_config(&config);
  config.fd = fd;
  config.enabled = 1;
  log_client_config(&config);
  (void)setup_globals();

  upload_process(&config, &upload_callback);
  printf("stopping upload daemon\n");
  teardown_globals();
  close(fd);
  unlink(fifo);
  exit(EXIT_FAILURE);
}
