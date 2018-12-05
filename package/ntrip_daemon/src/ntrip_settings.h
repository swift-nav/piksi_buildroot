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

#ifndef __NTRIP_H
#define __NTRIP_H

#include <libpiksi/settings_client.h>

void ntrip_settings_init(pk_settings_ctx_t *settings_ctx);
bool ntrip_reconnect(void);
void ntrip_stop_processes(void);
void ntrip_record_exit(pid_t pid);

#endif
