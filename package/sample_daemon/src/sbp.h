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

#ifndef SWIFTNAV_SBP_H
#define SWIFTNAV_SBP_H

#include <libsbp/sbp.h>
#include <libpiksi/settings.h>

// clang-format off
#define TIME_SOURCE_MASK 0x07 /* Bits 0-2 */
#define NO_TIME          0
// clang-format on

int sbp_init(void);
void sbp_deinit(void);
settings_ctx_t *sbp_get_settings_ctx(void);
int sbp_callback_register(u16 msg_type, sbp_msg_callback_t cb, void *context);
int sbp_run(void);

#endif /* SWIFTNAV_SBP_H */
