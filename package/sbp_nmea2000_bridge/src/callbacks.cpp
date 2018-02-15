#include <algorithm>
#include <iomanip>
#include <iostream>
#include <linux/can.h>

#include <N2kMessages.h>

extern "C" {
#include <libsbp/navigation.h>
#include <libsbp/orientation.h>
#include <libsbp/tracking.h>
}

#include "callbacks.h"
#include "common.h"
#include "SbpToNmea2kConverter.h"

namespace {
    SbpToNmea2kConverter converter;

    u8 tow_to_sid(const u32 tow) {
      // 251 is prime and close to the SID limit of 252.
      return tow % 251;
    }

    // Cache some info for composite N2k messages.
    // This makes the SBP -> NMEA2000 conversion stateful.
    class N2kCompositeMessageCache {
    public:
        N2kCompositeMessageCache() = default;

        void ResetIfOld(u32 tow) {
          constexpr u32 cOverflowDiff = 1000 * 60 * 60;  // One hour in ms.
          if((tow > tow_) ||  // Check if new tow or if overflown.
             ((tow < tow_) && ((tow_ - tow) <= cOverflowDiff))) {
            tow_ = tow;
            Reset();
          }
        }

        bool IsReadyToSend(bool is_valid) {
          // Checking ((tow_ % 10000) / 100) limits divider to two digits.
          if(is_valid) {
            divider_counter_ = ((tow_ % 10000) / 100) % cDivider;
            return utc_time_set_ && pos_llh_set_ && dops_set_ &&
                   divider_counter_ == 0;
          } else {  // This should be hit only by a single SBP message.
            divider_counter_ = (divider_counter_ + 1) % cDivider;
            return divider_counter_ == 0;
          }
        }

        void SetFields(const u32 tow, const u16 days_since_1970,
                       const double seconds_since_midnight) {
          if(tow == tow_) {
            days_since_1970_ = days_since_1970;
            seconds_since_midnight_ = seconds_since_midnight;
            utc_time_set_ = true;
          }
        }

        void SetFields(const msg_pos_llh_t *msg) {
          if(msg->tow == tow_) {
            lat_ = msg->lat;
            lon_ = msg->lon;
            alt_ = msg->height;
            sat_count_ = msg->n_sats;  // Sats used in solution.
            gnss_metod_ = gnss_method_lut_[msg->flags & 0x07];
            pos_llh_set_ = true;
          }
        }

        void SetFields(const msg_dops_t *msg) {
          if (msg->tow == tow_) {
            hdop_ = static_cast<double>(msg->hdop) / 100.0;
            pdop_ = static_cast<double>(msg->pdop) / 100.0;
            dops_set_ = true;
          }
        }

        // PGN 129029
        void FillN2kMsg(tN2kMsg *msg, bool is_valid) {
          d << __FUNCTION__ << "\n";

          if(is_valid) {
            d << "\tSats used in solution: " << static_cast<u16>(sat_count_)
              << "\n";
            SetN2kGNSS(*msg, tow_to_sid(tow_), days_since_1970_,
                       seconds_since_midnight_, lat_, lon_, alt_,
                       tN2kGNSStype::N2kGNSSt_GPSGLONASS,
                       tN2kGNSSmethod::N2kGNSSm_PreciseGNSS, sat_count_, hdop_,
                       pdop_, /*GeoidalSeparation=*/N2kDoubleNA);
            // We've hardcoded reference station count to zero.
            // That's a lie when RTK is in use. We're happy with it, for now.

            // Have to do GNSS method manually. It's not reverse engineered yet.
            msg->Data[31] = gnss_metod_;
            // Have to set integrity manually. 6 bits reserved. 2 bits set to 0.
            // That's why there are two values binary ORed together.
            // 0: No integrity checking.
            msg->Data[32] = 0xFC | 0x00;
          } else {
            SetN2kGNSS(*msg, /*sequence ID=*/0xFF,
                    /*DaysSince1970=*/N2kUInt16NA,
                    /*SecondsSinceMidnight=*/N2kDoubleNA,
                    /*Latitude=*/N2kDoubleNA, /*Longitude=*/N2kDoubleNA,
                    /*Altitude=*/N2kDoubleNA, tN2kGNSStype::N2kGNSSt_GPSGLONASS,
                       tN2kGNSSmethod::N2kGNSSm_PreciseGNSS,
                    /*nSatellites=*/N2kUInt8NA, /*HDOP=*/N2kDoubleNA,
                    /*PDOP=*/N2kDoubleNA, /*GeoidalSeparation=*/N2kDoubleNA);
            // Have to do GNSS method manually. It's not reverse engineered yet.
            msg->Data[31] = gnss_metod_;
            // Have to set integrity manually. 6 bits reserved. 2 bits set to 0.
            // That's why there are two values binary ORed together.
            // 0: No integrity checking.
            msg->Data[32] = 0xFC | 0x00;
          }

          d << "\tDone.\n";
        }
    private:
        void Reset() {
          utc_time_set_ = pos_llh_set_ = dops_set_ = false;
        }

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
        static constexpr u8 cDivider = 10;

        double seconds_since_midnight_ = 0.0;
        double lat_ = 0.0;
        double lon_ = 0.0;
        double alt_ = 0.0;
        double hdop_ = 0.0;
        double pdop_ = 0.0;
        u32 tow_ = 0;
        u16 days_since_1970_ = 0;
        u8 gnss_metod_ = 0;
        u8 sat_count_ = 0;  // Number of sats used in solution.
        u8 divider_counter_ = 0;

        bool utc_time_set_ = false;
        bool pos_llh_set_ = false;
        bool dops_set_ = false;
    };
    N2kCompositeMessageCache n2k_composite_message_cache;
    constexpr u8 N2kCompositeMessageCache::gnss_method_lut_[];
}

int callback_can_debug(zloop_t *loop, zmq_pollitem_t *item,
                       void *interface_name_void) {
  UNUSED(loop);
  auto interface_name = reinterpret_cast<const char*>(interface_name_void);

  canfd_frame frame;
  ssize_t bytes_read = recvfrom(item->fd, &frame,sizeof(frame),
          /*flags=*/0,
          /*src_addr=*/NULL,
          /*addrlen=*/NULL);
  piksi_check(bytes_read < 0, "Could not read interface %s.",
              interface_name);

  std::cout << "Read " << bytes_read << " bytes from " << interface_name
            <<" ID: " << std::setfill('0') << std::setw(4) << std::hex
            << frame.can_id
            << " Data: ";
  for (u32 i = 0; i < frame.len; ++i) {
    std::cout << std::setfill('0') << std::setw(2) << std::hex
              << static_cast<u16>(frame.data[i]);
  }
  std::cout << std::endl;

  return 0;
}

// PGN 126992
void callback_sbp_utc_time(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  auto sbp_utc_time = reinterpret_cast<msg_utc_time_t*>(msg);
  bool is_valid = (sbp_utc_time->flags & 0x07) != 0;

  tN2kMsg n2kMsg;
  u16 days_since_1970;
  double seconds_since_midnight;
  if (converter.Sbp259ToPgn126992(sbp_utc_time, &n2kMsg, &days_since_1970,
                                  &seconds_since_midnight)) {
    NMEA2000.SendMsg(n2kMsg);
  }

  converter.ResetPgn129029CacheIfOld(sbp_utc_time->tow);
  converter.SetPgn129029CacheFields(sbp_utc_time->tow, days_since_1970,
                                    seconds_since_midnight);
  if (is_valid && converter.IsPgn129029Ready(/*is_valid=*/true)) {
    n2kMsg.Clear();
    if (converter.Sbp259And520And522ToPgn129029(/*is_valid=*/true, &n2kMsg)) {
      NMEA2000.SendMsg(n2kMsg);
    }
  }
}

// PGN 127250
void callback_sbp_baseline_heading(u16 sender_id, u8 len, u8 msg[],
                                   void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  tN2kMsg n2kMsg;
  if(converter.Sbp527ToPgn127250(reinterpret_cast<msg_baseline_heading_t *>(msg),
                                 &n2kMsg)) {
    NMEA2000.SendMsg(n2kMsg);
  }
}

// PGN 129025
void callback_sbp_pos_llh(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  auto *sbp_pos = reinterpret_cast<msg_pos_llh_t*>(msg);
  bool is_valid = (sbp_pos->flags & 0x07) != 0;

  tN2kMsg n2kMsg;
  if (converter.Sbp522ToPgn129025(sbp_pos, &n2kMsg)) {
    NMEA2000.SendMsg(n2kMsg);
  }

  // In case of invalid position, pos_llh message marks an epoch.
  // Therefore, we don't check for validity here.
  converter.ResetPgn129029CacheIfOld(sbp_pos->tow);
  converter.SetPgn129029CacheFields(sbp_pos);
  if (converter.IsPgn129029Ready(is_valid)) {
    n2kMsg.Clear();
    if (converter.Sbp259And520And522ToPgn129029(is_valid, &n2kMsg)) {
      NMEA2000.SendMsg(n2kMsg);
    }
  }
}

// PGN 129026
void callback_sbp_vel_ned(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  tN2kMsg n2kMsg;
  if (converter.Sbp526ToPgn129026(reinterpret_cast<msg_vel_ned_t*>(msg),
                                  &n2kMsg)) {
    NMEA2000.SendMsg(n2kMsg);
  }
}

// PGN 129539
void callback_sbp_dops(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  auto sbp_dops = reinterpret_cast<msg_dops_t*>(msg);
  bool is_valid = (sbp_dops->flags & 0x07) != 0;

  tN2kMsg n2kMsg;
  if (converter.Sbp520ToPgn129539(sbp_dops, &n2kMsg)) {
    NMEA2000.SendMsg(n2kMsg);
  }

  converter.ResetPgn129029CacheIfOld(sbp_dops->tow);
  converter.SetPgn129029CacheFields(sbp_dops);
  if (is_valid &&
      converter.IsPgn129029Ready(/*is_valid=*/true)) {
    n2kMsg.Clear();
    if (converter.Sbp259And520And522ToPgn129029(/*is_valid=*/true, &n2kMsg)) {
      NMEA2000.SendMsg(n2kMsg);
    }
  }
}

// PGN 129540
void callback_sbp_tracking_state(u16 sender_id, u8 len, u8 msg[],
                                 void *context) {
  UNUSED(sender_id);
  UNUSED(context);

  tN2kMsg n2kMsg;
  if (converter.Sbp65ToPgn129540(reinterpret_cast<msg_tracking_state_t*>(msg),
                                len, &n2kMsg)) {
    NMEA2000.SendMsg(n2kMsg);
  }
}

void callback_sbp_heartbeat(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(msg);
  UNUSED(context);
  d << __FUNCTION__ << "\n";

  // This checks for new CAN messages and sends out a heartbeat.
  NMEA2000.ParseMessages();

  d << "\tDone.\n";
}