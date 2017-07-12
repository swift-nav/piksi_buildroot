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

int file_loop(const char *file_path)
{
  debug_printf("entered file_loop %s\n", file_path);
  int fd_read = open(file_path, O_RDONLY);
  if (fd_read < 0) {
    syslog(LOG_ERR, "error opening file");
    debug_printf("Error opening fd_read\n");
    return 1;
  }
  debug_printf("Opened %s for read, fd=%d\n", file_path, fd_read);

  int fd_write = open(file_path, O_WRONLY);
  if (fd_write < 0) {
    debug_printf("Error opening fd_write\n");
    return 1;
  }
  debug_printf("Open %s for write, fd=%d\n", file_path, fd_write);

  io_loop_start(fd_read, fd_write);
  io_loop_wait();

  close(fd_read);
  debug_printf("Closed fd_read %d\n", fd_read);

  close(fd_write);
  debug_printf("Closed fd_read %d\n", fd_write);

  return 0;
}
