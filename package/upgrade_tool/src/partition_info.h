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

#ifndef __PARTITION_INFO_H__
#define __PARTITION_INFO_H__

#include "common.h"

typedef struct {
  const char *name;
  uint32_t required_size;
} partition_config_t;

typedef struct {
  bool valid;
  uint32_t mtd_num;
  uint32_t offset;
  uint32_t size;
  uint32_t erasesize;
  uint32_t numeraseregions;
} partition_info_t;

int partition_info_table_populate(partition_info_t *info_table,
                                  const partition_config_t *config_table,
                                  uint32_t count);
int partition_info_table_verify(const partition_info_t *info_table,
                                const partition_config_t *config_table,
                                uint32_t count);

#endif /* __PARTITION_INFO_H__ */
