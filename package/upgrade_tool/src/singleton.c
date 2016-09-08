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

#include "singleton.h"

/* port unique to this application */
#define SINGLETON_PORT 10

int singleton_setup(void)
{
  /* create socket */
  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    printf("error creating singleton socket\n");
    return -1;
  }

  /* bind to address */
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SINGLETON_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    return -1;
  }

  return 0;
}
