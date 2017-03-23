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

#ifndef PIKSI_BUILDROOT_SBP_RTCM3_H
#define PIKSI_BUILDROOT_SBP_RTCM3_H

#include "rtcm3_messages.h"
#include <libsbp/observation.h>

#define MSG_OBS_P_MULTIPLIER             ((double)5e1)
#define MSG_OBS_CN0_MULTIPLIER           ((float)4)
#define MSG_OBS_LF_MULTIPLIER            ((double) (1 << 8))
#define MSG_OBS_DF_MULTIPLIER            ((double) (1 << 8))
#define MSG_OBS_FLAGS_CODE_VALID         ((u8) (1 << 0))
#define MSG_OBS_FLAGS_PHASE_VALID        ((u8) (1 << 1))
#define MSG_OBS_FLAGS_HALF_CYCLE_KNOWN   ((u8) (1 << 2))
#define MSG_OBS_FLAGS_MEAS_DOPPLER_VALID ((u8) (1 << 3))

/** Measurement flag: pseudorange and raw_pseudorange fields contain a
 *  valid value.
 *  \sa nav_meas_flags_t */
#define NAV_MEAS_FLAG_CODE_VALID         (1 << 0)
/** Measurement flag: carrier_phase and raw_carrier_phase fields contain a
 *  valid value.
 *  \sa nav_meas_flags_t */
#define NAV_MEAS_FLAG_PHASE_VALID        (1 << 1)
/** Measurement flag: measured_doppler and raw_measured_doppler fields
 *  contain a valid value.
 *  \sa nav_meas_flags_t */
#define NAV_MEAS_FLAG_MEAS_DOPPLER_VALID (1 << 2)
/** Measurement flag: computed_doppler and raw_computed_doppler fields
 *  contain a valid value.
 *  \sa nav_meas_flags_t */
#define NAV_MEAS_FLAG_COMP_DOPPLER_VALID (1 << 3)
/** Measurement flag: if bit not set, the half cycle carrier phase ambiguity has
 *  yet to be resolved. Carrier phase measurements might be 0.5 cycles out of
 *  phase.
 *  \sa nav_meas_flags_t */
#define NAV_MEAS_FLAG_HALF_CYCLE_KNOWN   (1 << 4)
/** Measurement flag: cn0 field contains a valid value.
 *  \sa nav_meas_flags_t */
#define NAV_MEAS_FLAG_CN0_VALID          (1 << 5)

#define SBP_HEADER_SIZE 11
#define SBP_OBS_SIZE 17
#define MAX_SBP_PAYLOAD 255
#define MAX_OBS_IN_SBP ( (MAX_SBP_PAYLOAD - SBP_HEADER_SIZE) / SBP_OBS_SIZE )

/** Code identifier. */
typedef enum code {
    CODE_INVALID = -1,
    CODE_GPS_L1CA = 0,
    CODE_GPS_L2CM = 1,
    CODE_SBAS_L1CA = 2,
    CODE_GLO_L1CA = 3,
    CODE_GLO_L2CA = 4,
    CODE_GPS_L1P = 5,
    CODE_GPS_L2P = 6,
    CODE_GPS_L2CL = 7,
    CODE_COUNT,
} code_t;

/** Number of milliseconds in a second. */
#define SECS_MS             1000

u8 encode_lock_time(double nm_lock_time);
double decode_lock_time(u8 sbp_lock_time);

/** Semi-major axis of the Earth, \f$ a \f$, in meters.
 * This is a defining parameter of the WGS84 ellipsoid. */
#define WGS84_A 6378137.0
/** Inverse flattening of the Earth, \f$ 1/f \f$.
 * This is a defining parameter of the WGS84 ellipsoid. */
#define WGS84_IF 298.257223563
/** The flattening of the Earth, \f$ f \f$. */
#define WGS84_F (1/WGS84_IF)
/** Semi-minor axis of the Earth in meters, \f$ b = a(1-f) \f$. */
#define WGS84_B (WGS84_A*(1-WGS84_F))
/** Eccentricity of the Earth, \f$ e \f$ where \f$ e^2 = 2f - f^2 \f$ */
#define WGS84_E (sqrt(2*WGS84_F - WGS84_F*WGS84_F))


static void wgsllh2ecef(const double llh[3], double ecef[3]);
static void wgsecef2llh(const double ecef[3], double llh[3]);


void rtcm3_decode_frame(const uint8_t *frame, uint32_t frame_length);

u8 rtcm3_obs_to_sbp( const rtcm_obs_message *rtcm_obs, msg_obs_t *sbp_obs[4], u8 sizes[4] );
void sbp_to_rtcm3_obs( const msg_obs_t *sbp_obs, const u8 msg_size, rtcm_obs_message *rtcm_obs );

void rtcm3_1005_to_sbp( const rtcm_msg_1005 *rtcm_1005, msg_base_pos_ecef_t *sbp_base_pos );
void sbp_to_rtcm3_1005( const msg_base_pos_ecef_t *sbp_base_pos, rtcm_msg_1005 *rtcm_1005 );

void rtcm3_1006_to_sbp( const rtcm_msg_1006 *rtcm_1006, msg_base_pos_ecef_t *sbp_base_pos );
void sbp_to_rtcm3_1006( const msg_base_pos_ecef_t *sbp_base_pos, rtcm_msg_1006 *rtcm_1006 );

#endif //PIKSI_BUILDROOT_SBP_RTCM3_H
