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
  int fd = open(file_path, O_RDWR);
  if (fd < 0) {
    printf("error opening TCP socket\n");
    return 1;
  }

  io_loop_start(fd);
  while(waitpid(-1, NULL, 0) >= 0) {
    ;
  }

  close(fd);
  fd = -1;
}
