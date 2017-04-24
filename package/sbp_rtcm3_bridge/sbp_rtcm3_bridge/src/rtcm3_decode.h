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

#ifndef SWIFTNAV_RTCM3_DECODE_H
#define SWIFTNAV_RTCM3_DECODE_H

#include "rtcm3_messages.h"
#include <stdbool.h>

u32 getbitu(const u8 *buff, u32 pos, u8 len);
u64 getbitul(const u8 *buff, u32 pos, u8 len);
s32 getbits(const u8 *buff, u32 pos, u8 len);
s64 getbitsl(const u8 *buff, u32 pos, u8 len);
void setbitu(u8 *buff, u32 pos, u32 len, u32 data);
void setbitul(u8 *buff, u32 pos, u32 len, u64 data);
void setbits(u8 *buff, u32 pos, u32 len, s32 data);
void setbitsl(u8 *buff, u32 pos, u32 len, s64 data);

#define RTCM3_PREAMBLE 0xD3   /**< RTCM v3 Frame sync / preamble byte. */
#define PRUNIT_GPS 299792.458 /**< RTCM v3 Unit of GPS Pseudorange (m) */

u16 rtcm3_write_header(const rtcm_msg_header *header, u8 num_sats, u8 *buff);
u16 rtcm3_read_header(const u8 *buff, rtcm_msg_header *header);
static u8 to_lock_ind(u32 time);
static u32 from_lock_ind(u8 lock);

u16 rtcm3_encode_1001(const rtcm_obs_message *rtcm_msg_1001, u8 *buff);
u16 rtcm3_encode_1002(const rtcm_obs_message *rtcm_msg_1002, u8 *buff);
u16 rtcm3_encode_1003(const rtcm_obs_message *rtcm_msg_1003, u8 *buff);
u16 rtcm3_encode_1004(const rtcm_obs_message *rtcm_msg_1004, u8 *buff);
u16 rtcm3_encode_1005_base(const rtcm_msg_1005 *rtcm_msg_1005, u8 *buff,
                           u16 *bit);
u16 rtcm3_encode_1005(const rtcm_msg_1005 *rtcm_msg_1005, u8 *buff);
u16 rtcm3_encode_1006(const rtcm_msg_1006 *rtcm_msg_1006, u8 *buff);
u16 rtcm3_encode_1007_base(const rtcm_msg_1007 *rtcm_msg_1007, u8 *buff,
                           u16 *bit);
u16 rtcm3_encode_1007(const rtcm_msg_1007 *rtcm_msg_1007, u8 *buff);
u16 rtcm3_encode_1008(const rtcm_msg_1008 *rtcm_msg_1008, u8 *buff);

s8 rtcm3_decode_1001(const u8 *buff, rtcm_obs_message *rtcm_msg_1001);
s8 rtcm3_decode_1002(const u8 *buff, rtcm_obs_message *rtcm_msg_1002);
s8 rtcm3_decode_1003(const u8 *buff, rtcm_obs_message *rtcm_msg_1003);
s8 rtcm3_decode_1004(const u8 *buff, rtcm_obs_message *rtcm_msg_1004);
s8 rtcm3_decode_1005_base(const u8 *buff, rtcm_msg_1005 *rtcm_msg_1005,
                          u16 *bit);
s8 rtcm3_decode_1005(const u8 *buff, rtcm_msg_1005 *rtcm_msg_1005);
s8 rtcm3_decode_1006(const u8 *buff, rtcm_msg_1006 *rtcm_msg_1006);
s8 rtcm3_decode_1007_base(const u8 *buff, rtcm_msg_1007 *rtcm_msg_1007,
                          u16 *bit);
s8 rtcm3_decode_1007(const u8 *buff, rtcm_msg_1007 *rtcm_msg_1007);
s8 rtcm3_decode_1008(const u8 *buff, rtcm_msg_1008 *rtcm_msg_1008);

static s8 decode_basic_l1_freq_data(const u8 *buff, u16 *bit,
                                    rtcm_freq_data *freq_data, u32 *pr,
                                    s32 *phr_pr_diff);
static s8 decode_basic_l2_freq_data(const u8 *buff, u16 *bit,
                                    rtcm_freq_data *freq_data, s32 *pr,
                                    s32 *phr_pr_diff);
static s8 encode_basic_freq_data(const rtcm_freq_data *freq_data,
                                 freq_enum freq, const double *l1_pr, u8 *buff,
                                 u16 *bit);
static void init_data(rtcm_sat_data *sat_data);

#endif /* SWIFTNAV_RTCM3_DECODE_H */
