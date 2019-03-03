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

#ifndef SWIFTNAV_ENDPOINT_ADAPTER_H
#define SWIFTNAV_ENDPOINT_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>

enum {
  IO_LOOP_ERROR = -2,
  IO_LOOP_STOP = -1,
  IO_LOOP_SUCCESS = 0,
};

int io_loop_run(int read_fd, int write_fd);

extern bool debug;

#define debug_printf(format, ...)                             \
  {                                                           \
    if (debug) PK_LOG_ANNO(LOG_DEBUG, format, ##__VA_ARGS__); \
  }

#endif /* SWIFTNAV_ENDPOINT_ADAPTER_H */
