/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_PORTS_H
#define SWIFTNAV_PORTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <libpiksi/settings.h>

int ports_init(settings_ctx_t *settings_ctx);
void ports_sigchld_waitpid_handler(pid_t pid, int status);

#endif /* SWIFTNAV_PORTS_H */
