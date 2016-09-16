/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __UPGRADE_DATA_H__
#define __UPGRADE_DATA_H__

#include "common.h"

int upgrade_data_load(const char *filename, const void **upgrade_data,
                      uint32_t *upgrade_data_length);
int upgrade_data_release(const void *upgrade_data);
int upgrade_data_verify(const void *upgrade_data, uint32_t upgrade_data_length);

#endif /* __UPGRADE_DATA_H__ */
