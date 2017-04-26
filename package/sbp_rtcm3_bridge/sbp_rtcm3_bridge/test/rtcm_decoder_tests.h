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

#ifndef PIKSI_BUILDROOT_RTCM_DECODER_TESTS_H
#define PIKSI_BUILDROOT_RTCM_DECODER_TESTS_H

#include <rtcm3_messages.h>

static void test_rtcm_encoder_decoder();

bool msgobs_equals(const rtcm_obs_message *msg_in,
                   const rtcm_obs_message *msg_out);
bool msg1005_equals(const rtcm_msg_1005 *lhs, const rtcm_msg_1005 *rhs);
bool msg1006_equals(const rtcm_msg_1006 *lhs, const rtcm_msg_1006 *rhs);
bool msg1007_equals(const rtcm_msg_1007 *lhs, const rtcm_msg_1007 *rhs);
bool msg1008_equals(const rtcm_msg_1008 *lhs, const rtcm_msg_1008 *rhs);

#endif // PIKSI_BUILDROOT_RTCM_DECODER_TESTS_H
