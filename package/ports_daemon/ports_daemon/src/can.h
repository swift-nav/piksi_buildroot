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

#ifndef SWIFTNAV_CAN_H
#define SWIFTNAV_CAN_H

#include <libpiksi/settings_client.h>

int can_init(pk_settings_ctx_t *settings_ctx);
u32 can_get_id(u8 can_number);
u32 can_get_filter(u8 can_number);

#endif /* SWIFTNAV_CAN_H */
