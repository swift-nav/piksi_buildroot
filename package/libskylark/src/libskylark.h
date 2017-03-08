/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_LIBSKYLARK_H
#define SWIFTNAV_LIBSKYLARK_H

#include <stdbool.h>

typedef struct {
  const char * const url;
  const char * const uuid;
  bool debug;
  int fd;
} skylark_config_t;

int skylark_setup(void);

void skylark_teardown(void);

int skylark_download(const skylark_config_t * const config);

int skylark_upload(const skylark_config_t * const config);

#endif /* SWIFTNAV_LIBSKYLARK_H */
