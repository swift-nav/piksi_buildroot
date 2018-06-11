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

#include "endpoint_adapter.h"

static int socket_create(int port)
{
  int ret;

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
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

  return fd;

err:
  close(fd);
  fd = -1;
  return ret;
}

int udp_listen_loop(int port)
{
  int fd = socket_create(port);
  if (fd < 0) {
    debug_printf("error listening UDP socket\n");
    return 1;
  }

  io_loop_start(fd, -1);
  io_loop_wait();
  close(fd);

  return 0;
}
