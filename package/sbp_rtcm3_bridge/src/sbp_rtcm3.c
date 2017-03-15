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

#include "sbp_rtcm3.h"

void rtcm3_to_sbp( const rtcm_obs_message *rtcm_obs, msg_obs_t *sbp_obs ) {
    sbp_obs->header.t.wn = thisweek;
    sbp_obs->header.t.tow = rtcm_obs->header.tow;
    sbp_obs->header.t.ns = 0.0;

    u8 index = 0;
    for( u8 sat = 0; sat < rtcm_obs->header.n_sat; ++sat ) {
        for( u8 freq = 0; freq < NUM_FREQS; ++freq ) {
            packed_obs_content_t *sbp_freq = &sbp_obs->obs[index];
            const rtcm_freq_data *rtcm_freq = &rtcm_obs->sats[sat].obs[freq];
            if( rtcm_freq->flags.valid_pr == 1 || rtcm_freq->flags.valid_cp == 1
                || rtcm_freq->flags.valid_cnr == 1 || rtcm_freq->flags.valid_lock == 1 ) {
                sbp_freq->sid.sat = rtcm_obs->sats[sat].svId;
                sbp_freq->sid.code = rtcm_freq->code;
                sbp_freq->flags = 0;
                sbp_freq->P = 0.0;
                sbp_freq->L.i = 0;
                sbp_freq->L.f = 0.0;
                sbp_freq->cn0 = 0.0;
                sbp_freq->lock = 0.0;
                sbp_freq->sid.code = freq;
                if( rtcm_freq->flags.valid_pr == 1 ) {
                    sbp_freq->P = rtcm_freq->pseudorange * MSG_OBS_P_MULTIPLIER;
                    sbp_freq->flags |= NAV_MEAS_FLAG_CODE_VALID;
                }
                if( rtcm_freq->flags.valid_cp == 1 ) {
                    sbp_freq->L.i = (s32)rtcm_freq->carrier_phase;
                    sbp_freq->L.f = (u8)((rtcm_freq->carrier_phase-sbp_freq->L.i)*MSG_OBS_LF_MULTIPLIER);
                    // really???
                    sbp_freq->L.i = -sbp_freq->L.i;
                    sbp_freq->L.f = -sbp_freq->L.f;
                    sbp_freq->flags |= NAV_MEAS_FLAG_PHASE_VALID;
                }
                if( rtcm_freq->flags.valid_cnr == 1 ) {
                    sbp_freq->cn0 = rtcm_freq->cnr * MSG_OBS_CN0_MULTIPLIER;
                    sbp_freq->flags |= NAV_MEAS_FLAG_CN0_VALID;
                }
                if( rtcm_freq->flags.valid_lock == 1 ) {
                    sbp_freq->lock = encode_lock_time(rtcm_freq->lock);
                    // not necessary???
                    //sbp_freq->flags += 1;
                }
                ++index;
            }
        }
    }
    sbp_obs->header.n_obs = index;
}

void sbp_to_rtcm3( const msg_obs_t *sbp_obs, rtcm_obs_message *rtcm_obs ) {
    rtcm_obs->header.tow = sbp_obs->header.t.tow;

    u8 count = 0;
    s8 sat2index[32] = {-1};
    for( u8 obs = 0; obs < sbp_obs->header.n_obs; ++obs ) {
        const packed_obs_content_t *sbp_freq = &sbp_obs->obs[obs];
        if( sbp_freq->flags != 0 ) {
            s8 sat_idx = sat2index[sbp_freq->sid.sat];
            if( sat_idx == -1 ) {
                rtcm_obs->sats[sat_idx].svId = sbp_freq->sid.sat;
                sat_idx = sat2index[sbp_freq->sid.sat] = count++;
            }
            // freq <-> code???
            rtcm_freq_data *rtcm_freq = &rtcm_obs->sats[sat_idx].obs[sbp_freq->sid.code];
            rtcm_freq->flags.data = 0;
            rtcm_freq->code = sbp_freq->sid.code;
            rtcm_freq->pseudorange = 0.0;
            rtcm_freq->carrier_phase = 0.0;
            rtcm_freq->cnr = 0.0;
            rtcm_freq->lock = 0.0;

            if( sbp_freq->flags & NAV_MEAS_FLAG_CODE_VALID ) {
                rtcm_freq->pseudorange = sbp_freq->P / MSG_OBS_P_MULTIPLIER;
                rtcm_freq->flags.valid_pr = 1;
            }
            if( sbp_freq->flags & NAV_MEAS_FLAG_PHASE_VALID ) {
                rtcm_freq->carrier_phase = -( sbp_freq->L.i + (double)sbp_freq->L.f / MSG_OBS_LF_MULTIPLIER );
                rtcm_freq->flags.valid_cp = 1;
            }
            if( sbp_freq->flags & NAV_MEAS_FLAG_CN0_VALID ) {
                rtcm_freq->cnr = sbp_freq->cn0 / MSG_OBS_CN0_MULTIPLIER;
                rtcm_freq->flags.valid_cnr = 1;
            }
            // not necessary???
//            if( sbp_freq->flags == 1 ) {
                rtcm_freq->lock = decode_lock_time(sbp_freq->lock);
                rtcm_freq->flags.valid_lock = 1;
//            }
        }
    }
    rtcm_obs->header.n_sat = count;
}

