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

#ifndef SWIFTNAV_PROTOCOLS_H
#define SWIFTNAV_PROTOCOLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef int (*port_adapter_opts_get_fn_t)(char *buf, size_t buf_size, const char *port_name);

typedef struct {
  const char *name;
  const char *setting_name;
  port_adapter_opts_get_fn_t port_adapter_opts_get;
} protocol_t;

int protocols_import(const char *path);
int protocols_count_get(void);
const protocol_t *protocols_get(int index);

#endif /* SWIFTNAV_PROTOCOLS_H */
