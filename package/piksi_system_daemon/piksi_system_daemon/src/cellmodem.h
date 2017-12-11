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

#ifndef __CELLMODEM_H
#define __CELLMODEM_H

#include <libpiksi/settings.h>

enum modem_type {
  MODEM_TYPE_INVALID,
  MODEM_TYPE_GSM,
  MODEM_TYPE_CDMA,
};

int cellmodem_init(sbp_zmq_pubsub_ctx_t *pubsub_ctx, settings_ctx_t *settings_ctx);
void cellmodem_set_dev(sbp_zmq_pubsub_ctx_t *pubsub_ctx, char *dev, enum modem_type type);
int pppd_respawn(zloop_t *loop, int timer_id, void *arg);

#endif
