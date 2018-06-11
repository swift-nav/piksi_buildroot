/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __FIRMWARE_STATE_H
#define __FIRMWARE_STATE_H

#include <libpiksi/sbp_rx.h>

enum spp_mode {
  SPP_MODE_INVALID = 0x00,
  SPP_MODE_SPP,
  SPP_MODE_DGNSS,
  SPP_MODE_FLOAT,
  SPP_MODE_FIXED,
  SPP_MODE_DEAD_RECK,
  SPP_MODE_SBAS
};

enum dgnss_mode {
  DGNSS_MODE_INVALID = 0x00,
  DGNSS_MODE_RESERVED,
  DGNSS_MODE_DGNSS,
  DGNSS_MODE_FLOAT,
  DGNSS_MODE_FIXED,
};

enum ins_mode {
  INS_MODE_NONE,
  INS_MODE_INS_USED
};

struct soln_state {
  struct {
    struct timespec systime;
    enum dgnss_mode mode;
  } dgnss;
  struct {
    struct timespec systime;
    enum spp_mode mode;
    enum ins_mode ins_mode;
  } spp;
  int sats;
  bool antenna;
};

void firmware_state_init(sbp_rx_ctx_t *ctx);
u8 firmware_state_obs_counter_get(void);
void firmware_state_get(struct soln_state *);
bool firmware_state_heartbeat_seen(void);

#endif
