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

#include <libpiksi/loop.h>
#include <libpiksi/settings.h>

enum modem_type {
  MODEM_TYPE_INVALID,
  MODEM_TYPE_GSM,
  MODEM_TYPE_CDMA,
};

int cell_modem_init(pk_loop_t *loop, settings_ctx_t *settings_ctx);
void cell_modem_deinit(void);
void cell_modem_set_dev(char *dev, enum modem_type type);
void pppd_respawn(pk_loop_t *loop, void *timer_handle, void *context);
void override_probe_retry(pk_loop_t *loop, void *timer_handle, void *context);
bool cell_modem_enabled(void);
char * cell_modem_get_dev_override(void);

#endif
