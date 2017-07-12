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

#include "zmq_adapter.h"

#define CONNECT_RETRY_TIME_s 10

static int addr_parse(const char *addr, struct sockaddr_in *s_addr)
{
  char ip[16];
  int port;
  if (sscanf(addr, "%15[^:]:%d", ip, &port) != 2) {
    syslog(LOG_ERR, "error parsing address");
    return 1;
  }

  memset(s_addr, 0, sizeof(*s_addr));
  s_addr->sin_family = AF_INET;
  s_addr->sin_port = htons(port);

  if (inet_aton(ip, &s_addr->sin_addr) == 0) {
    syslog(LOG_ERR, "invalid address");
    return 1;
  }

  return 0;
}

static int socket_create(const struct sockaddr_in *addr)
{
  int ret;

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return fd;
  }

  ret = connect(fd, (struct sockaddr *)addr, sizeof(*addr));
  if (ret != 0) {
    goto err;
  }

  return fd;

err:
  close(fd);
  fd = -1;
  return ret;
}

int tcp_connect_loop(const char *addr)
{
  struct sockaddr_in s_addr;
  if (addr_parse(addr, &s_addr) != 0) {
    return 1;
  }

  while (1) {
    int fd = socket_create(&s_addr);
    if (fd < 0) {
      debug_printf("error connecting TCP socket\n");
      sleep(CONNECT_RETRY_TIME_s);
      continue;
    }

    int wfd = dup(fd);
    io_loop_start(fd, wfd);
    io_loop_wait_one();
    io_loop_terminate();
    close(fd);
    close(wfd);
    fd = -1;
  }
}
