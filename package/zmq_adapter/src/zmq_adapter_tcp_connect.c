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

static int addr_parse(const char *addr, struct sockaddr *s_addr, socklet_t *s_addr_len)
{
  char hostname[256];
  int port;
  if (sscanf(addr, "%255[^:]:%d", hostname, &port) != 2) {
    syslog(LOG_ERR, "error parsing address");
    return 1;
  }

  struct addrinfo *resolutions = NULL;
  if (getaddrinfo(hostname, NULL, NULL, &resolutions) != 0) {
    syslog(LOG_ERR, "address resolution failed");
    return 1;
  }

  memcpy(s_addr, resolutions->ai_addr, resolutions->ai_addrlen);
  freeaddrinfo(resolutions);

  return 0;
}

static int socket_create(const struct sockaddr *addr, socklet_t addr_len)
{
  int ret;

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return fd;
  }

  ret = connect(fd, addr, addr_len);
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
  struct sockaddr_storage s_addr;
  socklet_t s_addr_len;
  if (addr_parse(addr, &s_addr, &s_addr_len) != 0) {
    return 1;
  }

  while (1) {
    int fd = socket_create((struct sockaddr *) &s_addr, s_addr_len);
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
