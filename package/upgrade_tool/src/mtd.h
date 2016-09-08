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

#ifndef __MTD_H__
#define __MTD_H__

#include "common.h"
#include "partition_info.h"

int mtd_erase(const partition_info_t *partition_info, uint32_t offset,
              uint32_t length);
int mtd_write_and_verify(const partition_info_t *partition_info,
                         uint32_t offset, const void *data, uint32_t length);
int mtd_read(const partition_info_t *partition_info,
             uint32_t offset, void *buffer, uint32_t length);
int mtd_crc_compute(const partition_info_t *partition_info,
                    uint32_t offset, uint32_t length, uint32_t *crc);

#endif /* __MTD_H__ */
