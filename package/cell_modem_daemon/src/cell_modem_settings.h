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

#ifndef __CELL_MODEM_SETTINGS_H
#define __CELL_MODEM_SETTINGS_H

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/settings.h>

enum modem_type {
  MODEM_TYPE_INVALID,
  MODEM_TYPE_GSM,
  MODEM_TYPE_CDMA,
};

int cell_modem_init(sbp_zmq_pubsub_ctx_t *pubsub_ctx, settings_ctx_t *settings_ctx);
void cell_modem_set_dev(char *dev, enum modem_type type);
int pppd_respawn(zloop_t *loop, int timer_id, void *arg);
int override_probe_retry(zloop_t *loop, int timer_id, void *arg);
bool cell_modem_enabled(void);
char * cell_modem_get_dev_override(void);

#endif
