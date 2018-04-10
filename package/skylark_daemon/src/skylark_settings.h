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

#ifndef __SKYLARK_H
#define __SKYLARK_H

#include <libpiksi/settings.h>

#define REQ_FIFO_NAME "/var/run/skylark/download/request"
#define REP_FIFO_NAME "/var/run/skylark/download/response"

void skylark_init(settings_ctx_t *settings_ctx);
bool skylark_reconnect_dl(void);

#endif
