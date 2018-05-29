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

#ifndef SWIFTNAV_ENDPOINT_ADAPTER_H
#define SWIFTNAV_ENDPOINT_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>
#include <unistd.h>

#include <czmq.h>

void io_loop_start(int read_fd, int write_fd);
void io_loop_wait(void);
void io_loop_wait_one(void);
void io_loop_terminate(void);

extern bool debug;

#define debug_printf(format, ...) \
  if (debug) \
    fprintf(stdout, "[PID %d] %s+%d(%s) " format, getpid(), \
      __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);

#endif /* SWIFTNAV_ENDPOINT_ADAPTER_H */
