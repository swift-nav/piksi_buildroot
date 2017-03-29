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

#include <libskylark.h>

#define VERBOSE

//
// Download Daemon - connects to Skylark and receives SBP messages.
//

static size_t download_callback(void *p, size_t size, size_t n, void *up)
{
  int *fd = (int *)up;
  ssize_t m = write(*fd, p, size * n);
  return m;
}

// TODO (mookerji): Command line stuff?

int main(void)
{
  log_info("starting download daemon\n");
  int fd;
  char * fifo = "/tmp/skylark_download";
  mkfifo(fifo, 0666);
  client_config_t config;
  (void)init_config(&config);
  fd = open(fifo, O_WRONLY);
  config.fd = fd;
  config.enabled = 1;
  log_client_config(&config);
  (void)setup_globals();
  download_process(&config, &download_callback);
  log_info("stopping download daemon\n");
  close(fd);
  unlink(fifo);
  teardown_globals();
  exit(EXIT_FAILURE);
}
