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

/** Represents all the relevant information about the signal
 *
 * Signal identifier containing constellation, band, and satellite identifier
 */
typedef struct __attribute__((packed)) {
    u8 sat;     /**< Constellation-specific satellite identifier */
    u8 code;    /**< Signal constellation, band and code */
} gnss_signal16_t;

/** Nanosecond-accurate receiver clock time
 *
 * A wire-appropriate receiver clock time, defined as the time
 * since the beginning of the week on the Saturday/Sunday
 * transition. In most cases, observations are epoch aligned
 * so ns field will be 0.
 */
typedef struct __attribute__((packed)) {
    u32 tow;    /**< Milliseconds since start of GPS week [ms] */
    s32 ns;     /**< Nanosecond residual of millisecond-rounded TOW (ranges
from -500000 to 500000)
 [ns] */
    u16 wn;     /**< GPS week number [week] */
} gps_time_nano_t;


/** GNSS carrier phase measurement.
 *
 * Carrier phase measurement in cycles represented as a 40-bit
 * fixed point number with Q32.8 layout, i.e. 32-bits of whole
 * cycles and 8-bits of fractional cycles. This phase has the
 * same sign as the pseudorange.
 */
typedef struct __attribute__((packed)) {
    s32 i;    /**< Carrier phase whole cycles [cycles] */
    u8 f;    /**< Carrier phase fractional part [cycles / 256] */
} carrier_phase_t;


/** Header for observation message.
 *
* Header of a GNSS observation message.
 */
typedef struct __attribute__((packed)) {
    gps_time_nano_t t;        /**< GNSS time of this observation */
    u8 n_obs;    /**< Total number of observations. First nibble is the size
of the sequence (n), second nibble is the zero-indexed
counter (ith packet of n)
 */
} observation_header_t;


/** GNSS doppler measurement.
 *
 * Doppler measurement in Hz represented as a 24-bit
 * fixed point number with Q16.8 layout, i.e. 16-bits of whole
 * doppler and 8-bits of fractional doppler. This doppler is defined
 * as positive for approaching satellites.
 */
typedef struct __attribute__((packed)) {
    s16 i;    /**< Doppler whole Hz [Hz] */
    u8 f;    /**< Doppler fractional part [Hz / 256] */
} doppler_t;


/** GNSS observations for a particular satellite signal.
 *
 * Pseudorange and carrier phase observation for a satellite being
 * tracked. The observations are interoperable with 3rd party
 * receivers and conform with typical RTCMv3 GNSS observations.
 */
typedef struct __attribute__((packed)) {
    u32 P;        /**< Pseudorange observation [2 cm] */
    carrier_phase_t L;        /**< Carrier phase observation with typical sign convention. [cycles] */
    doppler_t D;        /**< Doppler observation with typical sign convention. [Hz] */
    u8 cn0;      /**< Carrier-to-Noise density.  Zero implies invalid cn0. [dB Hz / 4] */
    u8 lock;     /**< Lock timer. This value gives an indication of the time
for which a signal has maintained continuous phase lock.
Whenever a signal has lost and regained lock, this
value is reset to zero. It is encoded according to DF402 from
the RTCM 10403.2 Amendment 2 specification.  Valid values range
from 0 to 15 and the most significant nibble is reserved for future use.
 */
    u8 flags;    /**< Measurement status flags. A bit field of flags providing the
status of this observation.  If this field is 0 it means only the Cn0
estimate for the signal is valid.
 */
    gnss_signal16_t sid;      /**< GNSS signal identifier (16 bit) */
} packed_obs_content_t;


/** GPS satellite observations
 *
 * The GPS observations message reports all the raw pseudorange and
 * carrier phase observations for the satellites being tracked by
 * the device. Carrier phase observation here is represented as a
 * 40-bit fixed point number with Q32.8 layout (i.e. 32-bits of
 * whole cycles and 8-bits of fractional cycles). The observations
 * are be interoperable with 3rd party receivers and conform
 * with typical RTCMv3 GNSS observations.
 */

#define SBP_MSG_OBS                  0x004A
typedef struct __attribute__((packed)) {
    observation_header_t header;    /**< Header of a GPS observation message */
    packed_obs_content_t obs[32];    /**< Pseudorange and carrier phase observation for a
satellite being tracked.
 */
} msg_obs_t;

/** Base station position in ECEF
 *
 * The base station position message is the position reported by
 * the base station itself in absolute Earth Centered Earth Fixed
 * coordinates. It is used for pseudo-absolute RTK positioning, and
 * is required to be a high-accuracy surveyed location of the base
 * station. Any error here will result in an error in the
 * pseudo-absolute position output.
 */
#define SBP_MSG_BASE_POS_ECEF        0x0048
typedef struct __attribute__((packed)) {
    double x;    /**< ECEF X coodinate [m] */
    double y;    /**< ECEF Y coordinate [m] */
    double z;    /**< ECEF Z coordinate [m] */
} msg_base_pos_ecef_t;

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



void rtcm3_obs_to_sbp( const rtcm_obs_message *rtcm_obs, msg_obs_t *sbp_obs );
void sbp_to_rtcm3_obs( const msg_obs_t *sbp_obs, rtcm_obs_message *rtcm_obs );

void rtcm3_1005_to_sbp( const rtcm_msg_1005 *rtcm_1005, msg_base_pos_ecef_t *sbp_base_pos );
void sbp_to_rtcm3_1005( const msg_base_pos_ecef_t *sbp_base_pos, rtcm_msg_1005 *rtcm_1005 );

void rtcm3_1006_to_sbp( const rtcm_msg_1006 *rtcm_1006, msg_base_pos_ecef_t *sbp_base_pos );
void sbp_to_rtcm3_1006( const msg_base_pos_ecef_t *sbp_base_pos, rtcm_msg_1006 *rtcm_1006 );

#endif //PIKSI_BUILDROOT_SBP_RTCM3_H
