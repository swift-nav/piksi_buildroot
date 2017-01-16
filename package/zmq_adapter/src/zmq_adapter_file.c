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

#include <termios.h>
#include <unistd.h>

#include "zmq_adapter.h"

int file_loop(const char *file_path)
{
  int fd = open(file_path, O_RDWR);
  if (fd < 0) {
    printf("error opening file\n");
    return 1;
  }

  if (isatty(fd)) {
    struct termios tio;
    tcgetattr(fd, &tio);
    cfmakeraw(&tio);
    tio.c_lflag &= ~ECHO;
    tio.c_oflag &= ~ONLCR;
    tcsetattr(fd, TCSANOW, &tio);
  }

  io_loop_start(fd);

  while (1) {
    int ret = waitpid(-1, NULL, 0);
    if ((ret == -1) && (errno == EINTR)) {
      /* Retry if interrupted */
      continue;
    } else if (ret >= 0) {
      /* Continue on success */
      continue;
    } else {
      /* Break on error */
      break;
    }
  }

  close(fd);
  fd = -1;
}
