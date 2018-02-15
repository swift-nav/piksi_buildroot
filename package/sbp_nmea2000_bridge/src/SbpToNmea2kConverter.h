#ifndef SBPTONMEA2KCONVERTER_H
#define SBPTONMEA2KCONVERTER_H

#include <N2kMessages.h>

extern "C" {
#include <libsbp/navigation.h>
#include <libsbp/orientation.h>
#include <libsbp/tracking.h>
}

class SbpToNmea2kConverter {
public:
    bool Sbp522ToPgn129025(const msg_pos_llh_t *msg, tN2kMsg *n2kMsg);
    bool Sbp526ToPgn129026(const msg_vel_ned_t *msg, tN2kMsg *n2kMsg);
    bool Sbp259And520And522ToPgn129029(const bool is_valid, tN2kMsg *msg);
    bool Sbp520ToPgn129539(const msg_dops_t *msg, tN2kMsg *n2kMsg);

    bool Sbp65ToPgn129540(const msg_tracking_state_t *msg, const u8 len,
                          tN2kMsg *n2kMsg);

    void ResetPgn129029CacheIfOld(const u32 tow);
    bool IsPgn129029Ready(const bool is_valid);
    void SetPgn129029CacheFields(const u32 tow, const u16 days_since_1970,
                                 const double seconds_since_midnight);
    void SetPgn129029CacheFields(const msg_pos_llh_t *msg);
    void SetPgn129029CacheFields(const msg_dops_t *msg);

private:
    void ResetPgn129029Cache();

    u8 tow_to_sid(const u32 tow);


    // Maps SBP GNSS method code to N2K GNSS method code.
    // 0x2X is GPS + GLONASS type of system.
    // The values from the lookup table are bitfields, which is why the
    // order is reverse.
    // 0: Invalid               -> 0x20: GPS + GLONASS, no GPS
    // 1: Single Point Position -> 0x21: GPS + GLONASS, GNSS fix
    // 2: Differential GNSS     -> 0x22: GPS + GLONASS, DGNSS fix
    // 3: Float RTK             -> 0x25: GPS + GLONASS, RTK Float
    // 4: Fixed RTK             -> 0x24: GPS + GLONASS, RTK Fixed Integer
    static constexpr u8 gnss_method_lut_[] = {0x02, 0x12, 0x22, 0x52,
                                              0x42, 0x02, 0x02, 0x02};
    // Need to change logic in IsReadyToSend to support more than
    // two digit dividers.
    static constexpr u8 cDividerPgn129029 = 10;

    double seconds_since_midnight_cache_ = 0.0;
    double lat_cache_ = 0.0;
    double lon_cache_ = 0.0;
    double alt_cache_ = 0.0;
    double hdop_cache_ = 0.0;
    double pdop_cache_ = 0.0;
    u32 tow_cache_ = 0;
    u16 days_since_1970_cache_ = 0;
    u8 gnss_metod_cache_ = 0;
    u8 sat_count_cache_ = 0;  // Number of sats used in solution.
    u8 divider_pgn129029_counter_ = 0;
    bool utc_time_cached_ = false;
    bool pos_llh_cached_ = false;
    bool dops_cached_ = false;

    u8 last_sid = 0;

};


#endif  // SBPTONMEA2KCONVERTER_H
