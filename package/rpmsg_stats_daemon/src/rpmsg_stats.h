/*
 * Copyright (C) 2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swift-nav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_RPMSG_STATS_H
#define SWIFTNAV_RPMSG_STATS_H

#include <swiftnav/common.h>

#define RPMSG_STATS_SIGNATURE 0x53574654
#define RPMSG_STATS_EOM_SIGNATURE 0x54465753

typedef struct __attribute__((packed)) rpmsg_stats {
  u32 signature;
  u32 version;
  u32 dropped_bytes;
  u32 write_count;
  u32 write_bytes;
  u32 read_count;
  u32 read_bytes;
  u32 send_fails;
  u32 sem_timeouts;
  u32 sem_wakeups;
  u32 eom;
} rpmsg_stats_t;

#define RPMSG_STATS_INIT { \
  .signature = RPMSG_STATS_SIGNATURE, \
  .version = 1, \
  .eom = RPMSG_STATS_EOM_SIGNATURE, }

#endif /* SWIFTNAV_RPMSG_STATS_H */
