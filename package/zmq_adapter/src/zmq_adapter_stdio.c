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

int stdio_loop(void)
{
  io_loop_start(STDIN_FILENO, STDOUT_FILENO);

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

  return 0;
}
