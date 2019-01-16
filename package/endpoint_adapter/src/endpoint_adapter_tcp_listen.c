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

#include "endpoint_adapter.h"

#define SOCKET_LISTEN_BACKLOG_LENGTH 16

static int socket_create(int port)
{
  int ret;

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return fd;
  }

  int opt_val = true;
  ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
  if (ret != 0) {
    goto err;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (ret != 0) {
    goto err;
  }

  ret = listen(fd, SOCKET_LISTEN_BACKLOG_LENGTH);
  if (ret != 0) {
    goto err;
  }

  return fd;

err:
  close(fd);
  fd = -1;
  return ret;
}

static void reap_children()
{
  /* Reap terminated child processes */
  while (waitpid(-1, NULL, WNOHANG) > 0) {
    /* No-op */
  }
}

static void server_loop(int server_fd)
{
  while (1) {
    reap_children();
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);

    if (client_fd >= 0) {
      int wfd = dup(client_fd);
      int rc = io_loop_run(client_fd, wfd, /* fork_needed = */ true);
      close(client_fd);
      close(wfd);
      client_fd = -1;
      if (rc != 0) break;
    } else if ((client_fd == -1) && (errno == EINTR)) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Break on error */
      break;
    }
  }
}

int tcp_listen_loop(int port)
{
  int server_fd = socket_create(port);
  if (server_fd < 0) {
    syslog(LOG_ERR, "error opening TCP socket");
    return 1;
  }

  server_loop(server_fd);

  close(server_fd);
  server_fd = -1;
  return 0;
}
