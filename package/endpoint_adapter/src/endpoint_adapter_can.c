/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/can/raw.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <libpiksi/logging.h>

#include "endpoint_adapter.h"

int can_loop(const char *can_name, u32 can_filter)
{
  while (1) {
    /* Open CAN socket */
    struct sockaddr_can addr;
    struct ifreq ifr;
    int socket_can = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    if (socket_can < 0) {
      piksi_log(LOG_ERR, "Could not open a socket for %s", can_name);
      return 1;
    }

    struct can_filter rfilter[1];
    rfilter[0].can_id = can_filter;
    rfilter[0].can_mask = CAN_SFF_MASK;

    if (setsockopt(socket_can, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter))) {
      piksi_log(LOG_ERR, "Could not set filter for %s", can_name);
      return 1;
    }

    const int loopback = 0;

    if (setsockopt(socket_can, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, sizeof(loopback))) {
      piksi_log(LOG_ERR, "Could not disable loopback for %s", can_name);
      return 1;
    }

    strcpy(ifr.ifr_name, can_name);
    if (ioctl(socket_can, SIOCGIFINDEX, &ifr) < 0) {
      piksi_log(LOG_ERR, "Could not get index name for %s", can_name);
      return 1;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(socket_can, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      piksi_log(LOG_ERR, "Could not bind %s", can_name);
      return 1;
    }

    /* Set socket for blocking */
    int flags = fcntl(socket_can, F_GETFL, 0);
    if (flags < 0) {
      piksi_log(LOG_ERR, "Failed CAN flag fetch");
      return 1;
    }

    if (fcntl(socket_can, F_SETFL, flags & ~O_NONBLOCK) < 0) {
      piksi_log(LOG_ERR, "Failed CAN flag set");
      return 1;
    }

    int wfd = dup(socket_can);
    io_loop_start_can(socket_can, wfd);
    io_loop_wait();
    io_loop_terminate();
    close(socket_can);
    close(wfd);
    socket_can = -1;
  }

  return 0;
}
