/*
 * Copyright (C) 2016-2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/logging.h>

#include "endpoint_adapter.h"

#define CONNECT_RETRY_TIME_s 10

typedef enum {
  SUCCESS,
  PARSE_FAILURE,
  RESOLVE_FAILURE,
  OTHER_ERROR

} process_addr_t;

static process_addr_t process_addr(const char *addr, struct sockaddr *s_addr, socklen_t *s_addr_len)
{
  char hostname[256] = {0};
  int port;
  if (sscanf(addr, "%255[^:]:%d", hostname, &port) != 2) {
    syslog(LOG_ERR, "error parsing address");
    return PARSE_FAILURE;
  }

  debug_printf("connecting to %s\n", hostname);

  struct addrinfo *resolutions = NULL;
  if (getaddrinfo(hostname, NULL, NULL, &resolutions) != 0) {
    syslog(LOG_ERR, "address resolution failed");
    return RESOLVE_FAILURE;
  }

  if (resolutions == NULL) {
    syslog(LOG_ERR, "no addresses returned by name resolution");
    return RESOLVE_FAILURE;
  }

  memcpy(s_addr, resolutions->ai_addr, resolutions->ai_addrlen);
  freeaddrinfo(resolutions);

  *s_addr_len = resolutions->ai_addrlen;

  if (resolutions->ai_family == AF_INET) {
    ((struct sockaddr_in *)s_addr)->sin_port = htons(port);
  } else if (resolutions->ai_family == AF_INET6) {
    ((struct sockaddr_in6 *)s_addr)->sin6_port = htons(port);
  } else {
    syslog(LOG_ERR, "unknown address family returned from name resolution");
    return OTHER_ERROR;
  }

  return SUCCESS;
}

static int socket_create(const struct sockaddr *addr, socklen_t addr_len)
{
  int ret;

  int fd = socket(addr->sa_family, SOCK_STREAM, 0);
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
  return ret;
}

static bool configure_socket(int fd)
{
  int ret = 0;
#ifdef TCP_USER_TIMEOUT
  unsigned int timeout = 5000;
  ret = setsockopt(fd, SOL_TCP, TCP_USER_TIMEOUT, &timeout, sizeof(timeout));
  if (ret < 0) {
    syslog(LOG_ERR, "setsockopt error %d", errno);
    return false;
  }
#endif
  int optval = 1;
  ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
  if (ret < 0) {
    syslog(LOG_ERR, "setsockopt (SO_KEEPALIVE) error: %d", errno);
    return false;
  }
  optval = 5;
  ret = setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &optval, sizeof(optval));
  if (ret < 0) {
    syslog(LOG_ERR, "setsockopt (TCP_KEEPCNT) error: %d", errno);
    return false;
  }
  optval = 5;
  ret = setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &optval, sizeof(optval));
  if (ret < 0) {
    syslog(LOG_ERR, "setsockopt (TCP_KEEPINTVL) error: %d", errno);
    return false;
  }
  optval = 20;
  ret = setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, &optval, sizeof(optval));
  if (ret < 0) {
    syslog(LOG_ERR, "setsockopt (TCP_KEEPIDLE) error: %d", errno);
    return false;
  }
  return true;
}

int tcp_connect_loop(const char *addr)
{
  struct sockaddr_storage s_addr;
  socklen_t s_addr_len = 0;

  while (1) {

    process_addr_t res = process_addr(addr, (struct sockaddr *)&s_addr, &s_addr_len);

    if (res == RESOLVE_FAILURE) {
      sleep(CONNECT_RETRY_TIME_s);
      continue;
    } else if (res != SUCCESS) {
      return 1;
    }

    int fd = socket_create((struct sockaddr *)&s_addr, s_addr_len);
    if (fd < 0) {
      debug_printf("error connecting TCP socket\n");
      sleep(CONNECT_RETRY_TIME_s);
      continue;
    }
    if (!configure_socket(fd)) {
      return 1;
    }
    int wfd = dup(fd);
    io_loop_run(fd, wfd);
    close(fd);
    close(wfd);
  }
}
