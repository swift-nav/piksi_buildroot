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
#include <libpiksi/loop.h>
#include <libpiksi/settings.h>

#define TIME_SOURCE_MASK 0x07 /* Bits 0-2 */
#define NO_TIME          0

int sbp_init(void);
void sbp_deinit_rover(void);
void sbp_deinit_reference(void);
settings_ctx_t * sbp_get_settings_ctx(void);
pk_loop_t * sbp_get_loop_rover(void);
pk_loop_t * sbp_get_loop_reference(void);
int sbp_callback_register_rover(u16 msg_type, sbp_msg_callback_t cb, void *context);
int sbp_callback_register_reference(u16 msg_type, sbp_msg_callback_t cb, void *context);
int sbp_run_rover(void);
int sbp_run_reference(void);

#endif /* SWIFTNAV_SBP_H */
