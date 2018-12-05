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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "endpoint_adapter.h"

static int is_pipe(const char *file_path)
{
  struct stat s;
  if (stat(file_path, &s)) {
    debug_printf("Cannot get stats for %s\n", file_path);
    return 0;
  } else {
    return S_ISFIFO(s.st_mode);
  }
}

int file_loop(const char *file_path, int need_read, int need_write)
{
  debug_printf("entered file_loop %s, need_read %d need write %d\n",
               file_path,
               need_read,
               need_write);

  int fd_read = -1, fd_write = -1;
  int b_pipe = is_pipe(file_path);
  debug_printf("b_pipe: %d\n", b_pipe);

  if (need_read) {
    if (b_pipe) {
      fd_read = open(file_path, O_RDWR);
    } else {
      fd_read = open(file_path, O_RDONLY);
    }
    if (fd_read < 0) {
      syslog(LOG_ERR, "error opening file: %s (%s)", file_path, strerror(errno));
      debug_printf("Error opening fd_read\n");
      return 1;
    }
    debug_printf("Opened %s for read, fd=%d\n", file_path, fd_read);
  }

  if (need_write) {
    if (b_pipe) {
      fd_write = open(file_path, O_RDWR);
    } else {
      fd_write = open(file_path, O_WRONLY);
    }
    if (fd_write < 0) {
      debug_printf("Error opening fd_write\n");
      return 1;
    }
    debug_printf("Open %s for write, fd=%d\n", file_path, fd_write);
  }

  io_loop_start(fd_read, fd_write);
  io_loop_wait();
  io_loop_terminate();

  if (need_read) {
    close(fd_read);
    debug_printf("Closed fd_read %d\n", fd_read);
  }

  if (need_write) {
    close(fd_write);
    debug_printf("Closed fd_read %d\n", fd_write);
  }

  return 0;
}
