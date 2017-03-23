//
// Created by pgrgich on 3/14/17.
//

#ifndef PIKSI_BUILDROOT_RTCM3_MESSAGES_H_H
#define PIKSI_BUILDROOT_RTCM3_MESSAGES_H_H

#include <stdint.h>
#include <libsbp/sbp.h>

#define RTCM_MAX_SATS 32

typedef enum {
    L1_FREQ,
    L2_FREQ,
    NUM_FREQS
} freq_enum;

static const double CLIGHT = 299792458.0;         /* speed of light (m/s) */
static const double FREQ1 = 1.57542e9;
static const double FREQ2 = 1.22760e9;

static const double FREQS[NUM_FREQS] = { 1.57542e9, 1.22760e9 }; /* L1/E1 and L2 frequencies (Hz) */

typedef struct {
    u16 msg_num;
    u16 stn_id;
    u32 tow;
    u8 sync;
    u8 n_sat;
    u8 div_free;
    u8 smooth;
} rtcm_msg_header;

typedef  union{
    struct {
        u8 valid_pr : 1;
        u8 valid_cp : 1;
        u8 valid_cnr : 1;
        u8 valid_lock : 1;
    };
    u8 data;
} flag_bf;

typedef struct {
    u8 code;
    double pseudorange;
    double carrier_phase;
    u32 lock;
    double cnr;
    flag_bf flags;

} rtcm_freq_data;

typedef struct {
    u8 svId;
    rtcm_freq_data obs[NUM_FREQS];
} rtcm_sat_data;

typedef struct {
    rtcm_msg_header header;
    rtcm_sat_data sats[RTCM_MAX_SATS];
} rtcm_obs_message;

typedef struct {
    u16 stn_id; //Reference Station ID DF003 uint12 12
    u8 ITRF; // Reserved for ITRF Realization Year DF021 uint6 6
    u8 GPS_ind; // GPS Indicator DF022 bit(1) 1
    u8 GLO_ind; // GLONASS Indicator DF023 bit(1) 1
    u8 GAL_ind; // Reserved for Galileo Indicator DF024 bit(1) 1
    u8 ref_stn_ind; //Reference-Station Indicator DF141 bit(1) 1
    double arp_x; // Antenna Reference Point ECEF-X DF025 int38 38
    u8 osc_ind; // Single Receiver Oscillator Indicator DF142 bit(1) 1
    u8 reserved; // Reserved DF001 bit(1) 1
    double arp_y; // Antenna Reference Point ECEF-Y DF026 int38 38
    u8 quart_cycle_ind; // Quarter Cycle Indicator DF364 bit(2) 2
    double arp_z; // Antenna Reference Point ECEF-Z DF027 int38 38
} rtcm_msg_1005;

typedef struct {
    rtcm_msg_1005 msg_1005;
    double ant_height; // Antenna Height DF028 uint16 16
} rtcm_msg_1006;

typedef struct {
    u16 stn_id; //Reference Station ID DF003 uint12 12
    u8 desc_count; // Descriptor Counter N DF029 uint8 8 N <= 31
    char desc[32]; // Antenna Descriptor DF030 char8(N) 8*N
    u8 ant_id; // Antenna Setup ID DF031 uint8 8
} rtcm_msg_1007;

typedef struct {
    rtcm_msg_1007 msg_1007;
    u8 serial_count; // Serial Number Counter M DF032 uint8 8 M <= 31
    char serial_num[32]; // Antenna Serial Number DF033 char8(M) 8*M
} rtcm_msg_1008;


#endif //PIKSI_BUILDROOT_RTCM3_MESSAGES_H_H
