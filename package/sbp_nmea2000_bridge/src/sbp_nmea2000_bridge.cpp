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

#include <algorithm>
#include <array>
#include <czmq.h>
#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/sockios.h>
#include <libsocketcan.h>
#include <net/if.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

extern "C" {
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/util.h>
#include <libpiksi/logging.h>
#include <libsbp/navigation.h>
#include <libsbp/orientation.h>
#include <libsbp/sbp.h>
#include <libsbp/system.h>
#include <libsbp/tracking.h>
}

#define USE_N2K_CAN 5
#include <N2kMessages.h>
#include <libpiksi/common.h>
#include <c/include/libsbp/orientation.h>
#include "NMEA2000_CAN.h"
#include "NMEA2000_SocketCAN.h"
#include "sbp.h"

#define UNUSED(x) (void)(x)

namespace {
    constexpr char cProgramName[] = "sbp_nmea2000_bridge";

    bool debug = false;
    int loopback = false;

    constexpr char cInterfaceNameCan0[] = "can0";
    constexpr char cInterfaceNameCan1[] = "can1";

    // Terminate with a zero.
    constexpr unsigned long cTransmitPGNs[] = {126992, 127250, 129025, 129026,
                                               129029, 129539, 129540, 0};

    u32 bitrate_can0 = 250 * 1000;
    u32 bitrate_can1 = 250 * 1000;

    constexpr int cEnable = 1;
    constexpr int cDisable = 0;

    // Socket where this process will take in SBP messages to be put on the CAN.
    constexpr char cEndpointSub[] = ">tcp://127.0.0.1:43090";
    // Socket where this process will output CAN wrapped in SBP.
    // Not used at present, actually.
    constexpr char cEndpointPub[] = ">tcp://127.0.0.1:43091";

    // Product information.
    constexpr u16 cNmeaNetworkMessageDatabaseVersion = 2100;
    constexpr u16 cNmeaManufacturersProductCode = 0xFFFF;  // Assigned by NMEA.
//    u8 cManufacturersModelId[32] = "";  // Piksi Multi or Piksi Duro or Piksi Nano.
//    u8 cManufacturersSoftwareVersionCode[32] = "";  // 1.3.something and so on.
//    u8 cManufacturersModelVersion[32] = "";  // Hardware revision?
    char manufacturers_model_serial_code[32] = "";
    constexpr char cModelSerialCodePath[] = "/factory/mfg_id";
    constexpr u8 cNMEA2000CertificationLevel = 2;  // Not applicable any more. Part of old standard.
    constexpr u8 cLoadEquivalency = 8;
    u8 last_sid = 0;

    struct debug_stream {
        template<typename T>
        debug_stream& operator<<(T arg) {
          if(debug) {
            std::cout << std::dec << arg;
          }
          return *this;
        }
    };
    debug_stream d;

    u8 tow_to_sid(const u32 tow) {
      // 251 is prime and close to the SID limit of 252.
      return (last_sid = tow % 251);
    }

    // Cache some info for composite N2k messages.
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

    void usage(char *command) {
      std::cout << "Usage: " << command << " [--bitrate N] [--debug] [--loopback]" << std::endl
                << "Where N is an acceptable CAN bitrate." << std::endl;

      std::cout << std::endl << "Misc options:" << std::endl;
      std::cout << "\t--debug" << std::endl;
    }

    int parse_options(int argc, char *argv[]) {
      enum {
          OPT_ID_DEBUG = 0,
          OPT_ID_BITRATE = 1,
          OPT_ID_LOOPBACK = 2,
      };

      constexpr option long_opts[] = {
              {"debug",    no_argument,       nullptr, OPT_ID_DEBUG},
              {"bitrate",  required_argument, nullptr, OPT_ID_BITRATE},
              {"loopback", no_argument,       nullptr, OPT_ID_LOOPBACK},
              {nullptr,    no_argument,       nullptr, 0}
      };

      int opt;
      while ((opt = getopt_long(argc, argv, "", long_opts, nullptr)) != -1) {
        switch (opt) {
          case OPT_ID_DEBUG:
            debug = true;
            break;
          case OPT_ID_BITRATE:
            bitrate_can0 = bitrate_can1 = static_cast<u32>(atoi(optarg));
            break;
          case OPT_ID_LOOPBACK:
            loopback = true;
            break;
          default:
            piksi_log(LOG_ERR, "Invalid option.");
            std::cout << "Invalid option." << std::endl;
            return -1;
        }
      }

      if(optind != argc){
        return -1;
      }

      return 0;
    }

    void calculate_cog_sog(const msg_vel_ned_t *sbp_vel_ned,
                           double *cog_rad, double *sog_mps) {
      // Millimeters to meters.
      double vel_north_mps = sbp_vel_ned->n / 1000.0;
      double vel_east_mps = sbp_vel_ned->e / 1000.0;

      *cog_rad = atan2(vel_east_mps, vel_north_mps);

      // Convert negative values to positive.
      constexpr double kFullCircleRad = 2 * 3.1415926535897932384626433832795;
      if (*cog_rad < 0.0) {
        *cog_rad += kFullCircleRad;
      }

      // Avoid having duplicate values for same point (0 and 360).
      if (fabs(kFullCircleRad - *cog_rad) < 10e-1) {
        *cog_rad = 0;
      }

      *sog_mps = sqrt(vel_north_mps * vel_north_mps +
                      vel_north_mps * vel_north_mps);
    }

    void calculate_days_and_seconds_since_1970(const msg_utc_time_t *sbp_utc_t,
                                               u16 *days_since_1970,
                                               double *seconds_since_midnight) {
      tm tm_utc_time;
      // 1900 is the starting year (year 0) in tm_year, so subtract 1900.
      tm_utc_time.tm_year = sbp_utc_t->year - 1900;
      // 0 is the starting month in tm_mon, so subtract 1.
      tm_utc_time.tm_mon = sbp_utc_t->month - 1;
      tm_utc_time.tm_mday = sbp_utc_t->day;
      tm_utc_time.tm_hour = sbp_utc_t->hours;
      tm_utc_time.tm_min = sbp_utc_t->minutes;
      tm_utc_time.tm_sec = sbp_utc_t->seconds;
      tm_utc_time.tm_isdst = -1;  // -1 means no idea. mktime() will know.
      time_t utc_time_since_epoch = mktime(&tm_utc_time);

      constexpr u32 cSecondsInADay = 60 * 60 * 24;
      *days_since_1970 =
              static_cast<u16>(utc_time_since_epoch / cSecondsInADay);
      *seconds_since_midnight =
              (utc_time_since_epoch % cSecondsInADay) + sbp_utc_t->ns * 1e-9;
    }

    void piksi_check(int err, const char* format, ...) {
      if (err != 0) {
        va_list ap;
        va_start(ap, format);
        piksi_log(LOG_ERR, format, ap);
        va_end(ap);
        if(debug) {
          printf(format, ap);
        }
        exit(EXIT_FAILURE);
      }
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
      d << __FUNCTION__ << "\n";

      auto sbp_utc_time = reinterpret_cast<msg_utc_time_t*>(msg);
      u16 days_since_1970;
      double seconds_since_midnight;
      calculate_days_and_seconds_since_1970(sbp_utc_time,
                                            &days_since_1970,
                                            &seconds_since_midnight);
      d << "\tTOW: " << sbp_utc_time->tow << "\n"
        << "\tDays since 1970: " << days_since_1970 << "\n"
        << "\tSeconds since midnight: " << seconds_since_midnight << "\n";

      bool is_valid = (sbp_utc_time->flags & 0x07) != 0;

      tN2kMsg N2kMsg;
      if (is_valid) {
        SetN2kSystemTime(N2kMsg, tow_to_sid(sbp_utc_time->tow),
                         days_since_1970, seconds_since_midnight,
                         tN2kTimeSource::N2ktimes_GPS);
      } else {
        SetN2kSystemTime(N2kMsg, /*sequence ID=*/0xFF,
                         N2kUInt16NA, N2kDoubleNA,
                         tN2kTimeSource::N2ktimes_GPS);
      }
      NMEA2000.SendMsg(N2kMsg);

      n2k_composite_message_cache.ResetIfOld(sbp_utc_time->tow);
      n2k_composite_message_cache.SetFields(sbp_utc_time->tow,
                                            days_since_1970,
                                            seconds_since_midnight);
      if (is_valid &&
          n2k_composite_message_cache.IsReadyToSend(/*is_valid=*/true)) {
        N2kMsg.Clear();
        n2k_composite_message_cache.FillN2kMsg(&N2kMsg, /*is_valid=*/true);
        NMEA2000.SendMsg(N2kMsg);
      }

      d << "\tDone.\n";
    }

    // PGN 127250
    void callback_sbp_baseline_heading(u16 sender_id, u8 len, u8 msg[],
                                       void *context){
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(context);
      d << __FUNCTION__ << "\n";

      auto sbp_baseline_heading =
              reinterpret_cast<msg_baseline_heading_t*>(msg);

      d << "\tBaseline heading in deg: "
        << sbp_baseline_heading->heading / 1000.0 << "\n"
        << "\tBaseline heading in rad: "
        << DegToRad(sbp_baseline_heading->heading / 1000.0) << "\n";

      bool is_valid = (sbp_baseline_heading->flags & 0x07) == 4;

      tN2kMsg N2kMsg;
      if (is_valid) {
        SetN2kTrueHeading(N2kMsg, tow_to_sid(sbp_baseline_heading->tow),
                          DegToRad(sbp_baseline_heading->heading / 1000.0));
      } else {
        SetN2kTrueHeading(N2kMsg, /*sequence ID=*/0xFF, N2kDoubleNA);
      }
      NMEA2000.SendMsg(N2kMsg);

      d << "\tDone.\n";
    }

    // PGN 129025
    void callback_sbp_pos_llh(u16 sender_id, u8 len, u8 msg[], void *context) {
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(context);
      d << __FUNCTION__ << "\n";

      auto *sbp_pos = reinterpret_cast<msg_pos_llh_t*>(msg);

      d << "\tTOW: " << sbp_pos->tow << "\n"
        << "\tLat: " << sbp_pos->lat << "\n"
        << "\tLon: " << sbp_pos->lon << "\n"
        << "\tAlt: " << sbp_pos->height << "\n";

      bool is_valid = (sbp_pos->flags & 0x07) != 0;

      tN2kMsg N2kMsg;
      if(is_valid) {
        SetN2kLatLonRapid(N2kMsg, sbp_pos->lat, sbp_pos->lon);
      } else {
        SetN2kLatLonRapid(N2kMsg, N2kDoubleNA, N2kDoubleNA);
      }
      NMEA2000.SendMsg(N2kMsg);

      // In case of invalid position, pos_llh message marks an epoch.
      // Therefore, we don't check for validity here.
      n2k_composite_message_cache.ResetIfOld(sbp_pos->tow);
      n2k_composite_message_cache.SetFields(sbp_pos);
      if (n2k_composite_message_cache.IsReadyToSend(is_valid)) {
        N2kMsg.Clear();
        n2k_composite_message_cache.FillN2kMsg(&N2kMsg, is_valid);
        NMEA2000.SendMsg(N2kMsg);
      }

      d << "\tDone.\n";
    }

    // PGN 129026
    void callback_sbp_vel_ned(u16 sender_id, u8 len, u8 msg[], void *context){
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(context);
      d << __FUNCTION__ << "\n";

      auto *sbp_vel_ned = reinterpret_cast<msg_vel_ned_t*>(msg);
      double cog_rad, sog_mps;
      calculate_cog_sog(sbp_vel_ned, &cog_rad, &sog_mps);

      d << "\tCourse over ground in rad: " << cog_rad << "\n"
        << "\tSpeed over ground in m/s: " << sog_mps << "\n";

      bool is_valid = (sbp_vel_ned->flags & 0x07) != 0;

      tN2kMsg N2kMsg;
      if(is_valid) {
        SetN2kCOGSOGRapid(N2kMsg, tow_to_sid(sbp_vel_ned->tow),
                          tN2kHeadingReference::N2khr_true,
                          cog_rad, sog_mps);
      } else {
        SetN2kCOGSOGRapid(N2kMsg, /*sequence ID=*/0xFF,
                          tN2kHeadingReference::N2khr_true,
                          N2kDoubleNA, N2kDoubleNA);
      }
      NMEA2000.SendMsg(N2kMsg);

      d << "\tDone.\n";
    }

    // PGN 129539
    void callback_sbp_dops(u16 sender_id, u8 len, u8 msg[], void *context){
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(context);
      d << __FUNCTION__ << "\n";

      auto sbp_dops = reinterpret_cast<msg_dops_t*>(msg);

      bool is_valid = (sbp_dops->flags & 0x07) != 0;

      tN2kMsg N2kMsg;
      if (is_valid) {
        SetN2kGNSSDOPData(N2kMsg, tow_to_sid(sbp_dops->tow),
                /*DesiredMode=*/tN2kGNSSDOPmode::N2kGNSSdm_Auto,
                /*ActualMode=*/tN2kGNSSDOPmode::N2kGNSSdm_3D,
                          sbp_dops->hdop / 100.0,
                          sbp_dops->vdop / 100.0,
                          sbp_dops->tdop / 100.0);
      } else {
        SetN2kGNSSDOPData(N2kMsg, /*sequence ID=*/0xFF,
                /*DesiredMode=*/tN2kGNSSDOPmode::N2kGNSSdm_Auto,
                /*ActualMode=*/tN2kGNSSDOPmode::N2kGNSSdm_Error,
                          N2kDoubleNA, N2kDoubleNA, N2kDoubleNA);
      }
      NMEA2000.SendMsg(N2kMsg);

      d << "\tTOW: " << sbp_dops->tow << "\n"
        << "\tHDOP: " << sbp_dops->hdop / 100.0 << "\n"
        << "\tVDOP: " << sbp_dops->vdop / 100.0 << "\n"
        << "\tTDOP: " << sbp_dops->tdop / 100.0 << "\n";

      n2k_composite_message_cache.ResetIfOld(sbp_dops->tow);
      n2k_composite_message_cache.SetFields(sbp_dops);
      if (is_valid &&
          n2k_composite_message_cache.IsReadyToSend(/*is_valid=*/true)) {
        N2kMsg.Clear();
        n2k_composite_message_cache.FillN2kMsg(&N2kMsg, /*is_valid=*/true);
        NMEA2000.SendMsg(N2kMsg);
      }

      d << "\tDone.\n";
    }

    // PGN 129540
    void callback_sbp_tracking_state(u16 sender_id, u8 len, u8 msg[],
                                     void *context) {
      UNUSED(sender_id);
      UNUSED(context);

      // Divide by the divisor. Very hacky to have it in a static.
      constexpr u8 cDivider = 2;
      static u8 divider_counter = 0;
      if ((++divider_counter % cDivider) != 0) {
        return;
      }

      d << __FUNCTION__ << "\n"
        << "\tGot " << static_cast<u16>(len) << "/"
        << sizeof(tracking_channel_state_t) << "="
        << static_cast<u16>(len / sizeof(tracking_channel_state_t))
        << " states. Message len is most likely statically allocated.\n";

      constexpr u8 cConstellationMaxSats = 32;
      std::array<std::pair<u8, u8>, cConstellationMaxSats> sats_gps = {};
      std::array<std::pair<u8, u8>, cConstellationMaxSats> sats_glo = {};
      auto sats_gps_end_it = sats_gps.begin();
      auto sats_glo_end_it = sats_glo.begin();
      auto sbp_tracking_state = reinterpret_cast<msg_tracking_state_t*>(msg);
      for(u8 i = 0; i < (len / sizeof(tracking_channel_state_t)); ++i) {
        auto tracking_channel_state = sbp_tracking_state->states[i];
        if(tracking_channel_state.sid.sat == 0) {  // Invalid sat.
          continue;
        }

        switch (tracking_channel_state.sid.code) {
          case 0:  // GPS satellites. Fallthrough.
          case 1:
          case 2:
          case 5:
          case 6:
            *sats_gps_end_it = std::make_pair(tracking_channel_state.sid.sat,
                                              tracking_channel_state.cn0);
            ++sats_gps_end_it;
            break;
          case 3:  // GLONASS satellites. Fallthrough.
          case 4:
            *sats_glo_end_it = std::make_pair(tracking_channel_state.sid.sat,
                                              tracking_channel_state.cn0);
            ++sats_glo_end_it;
            break;
          default:
            d << "\tControl should have never reached this point.\n";
            exit(-1);  // Should never happen.
        }
      }

      // Sort and remove duplicates.
      auto comparator_lower_than = [](const std::pair<u8, u8>& a,
                                      const std::pair<u8, u8>& b) {
          // First is the satellite ID, second is the CN0.
          a.first < b.first ? true : a.second > b.second;
      };
      auto comparator_equal = [](const std::pair<u8, u8>& a,
                                 const std::pair<u8, u8>& b) {
          return a.first == b.first;
      };
      std::sort(sats_gps.begin(), sats_gps_end_it, comparator_lower_than);
      std::sort(sats_glo.begin(), sats_glo_end_it, comparator_lower_than);
      sats_gps_end_it = std::unique(sats_gps.begin(), sats_gps_end_it,
                                    comparator_equal);
      sats_glo_end_it = std::unique(sats_glo.begin(), sats_glo_end_it,
                                    comparator_equal);

      // Triton2 screen that this was tested with acted weird in regards
      // to sat count:
      //  - up to 12 sats (inclusive), everything was displayed fine
      //  - from 13 to 17 sats (inclusive), number 12 would be shown on screen
      //  - from 18 and upwards, no sat count number would be shown
      // The problem has to do with the number of detailed sat infos actually
      // sent, not just the count that was sent.
      // This is the number of tracked satellites.
      u8 sat_count = sats_gps_end_it - sats_gps.begin() +
                     sats_glo_end_it - sats_glo.begin();
      d << "\tDeduplicated. " << static_cast<u16>(sat_count)
        << " sats left. GPS: " << sats_gps_end_it - sats_gps.begin()
        << " GLONASS: " << sats_glo_end_it - sats_glo.begin() << "\n";

      tN2kMsg N2kMsg;
      N2kMsg.SetPGN(129540L);
      N2kMsg.Priority=6;
      // This sequence ID is a guess. Have no tow to get a definite one.
      N2kMsg.AddByte(last_sid);
      // TODO: At present, we can't get range residuals.
      N2kMsg.AddByte(0xFF);
      N2kMsg.AddByte(sat_count);
      for(auto it = sats_gps.begin(); it != sats_gps_end_it; ++it) {
        N2kMsg.AddByte(/*PRN=*/it->first);
        // TODO: At present, we can't get azimuth and elevation.
        N2kMsg.Add2ByteUDouble(/*elevation=*/N2kDoubleNA, 0.0001, N2kDoubleNA);
        N2kMsg.Add2ByteUDouble(/*azimuth=*/N2kDoubleNA, 0.0001, N2kDoubleNA);
        N2kMsg.Add2ByteUDouble(/*CN0=*/it->second * 0.25, 0.01, N2kDoubleNA);
        // TODO: At present, we can't get range residuals.
        N2kMsg.Add4ByteUInt(/*rangeResiduals=*/N2kInt32NA);
        // TODO: At present, we can't tel if the satellite is used in the solution.
        // Byte contains two 4-bit fields. One is reserved. Other is PRN status.
        // 1: Tracked but not used in solution.
        N2kMsg.AddByte(/*tracked=*/0xF1);
      }
      for(auto it = sats_glo.begin(); it != sats_glo_end_it; ++it) {
        N2kMsg.AddByte(/*PRN=*/it->first + 64);
        // TODO: At present, we can't get azimuth and elevation.
        // Stefan: not in SBP, but see NMEA 183 message GSV implementation,
        // we have (should have) some there I think
        N2kMsg.Add2ByteUDouble(/*elevation=*/N2kDoubleNA, 0.0001, N2kDoubleNA);
        N2kMsg.Add2ByteUDouble(/*azimuth=*/N2kDoubleNA, 0.0001, N2kDoubleNA);
        N2kMsg.Add2ByteUDouble(/*CN0=*/it->second * 0.25, 0.01, N2kDoubleNA);
        // TODO: At present, we can't get range residuals.
        N2kMsg.Add4ByteUInt(/*rangeResiduals=*/N2kInt32NA);
        // TODO: At present, we can't tel if the satellite is used in the solution.
        // Byte contains two 4-bit fields. One is reserved. Other is PRN status.
        // 1: Tracked but not used in solution.
        N2kMsg.AddByte(/*tracked=*/0xF1);
      }
      NMEA2000.SendMsg(N2kMsg);

      d << "\tDone.\n";
    }

    void callback_sbp_heartbeat(u16 sender_id, u8 len, u8 msg[],
                                void *context) {
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(msg);
      UNUSED(context);
      d << __FUNCTION__ << "\n";

      // This checks for new CAN messages and sends out a heartbeat.
      NMEA2000.ParseMessages();

      d << "\tDone.\n";
    }
}  // namespace

int main(int argc, char *argv[]) {
  logging_init(cProgramName);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "Invalid arguments.");
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  // Get the current state of the CAN interfaces.
  int state_can0, state_can1;
  piksi_check(can_get_state(cInterfaceNameCan0, &state_can0),
              "Could not get %s state.", cInterfaceNameCan0);
  piksi_check(can_get_state(cInterfaceNameCan1, &state_can1),
              "Could not get %s state.", cInterfaceNameCan1);

  // Bring the CAN interfaces down if they're up.
  if(state_can0 != CAN_STATE_STOPPED){
    piksi_check(can_do_stop(cInterfaceNameCan0),
                "Could not stop %s.", cInterfaceNameCan0);
  }
  if(state_can1 != CAN_STATE_STOPPED){
    piksi_check(can_do_stop(cInterfaceNameCan1),
                "Could not stop %s.", cInterfaceNameCan1);
  }

  // Set the CAN bitrate. This must be done before turning them back on.
  d << "Setting bitrate to " << bitrate_can0 << " and " << bitrate_can1 << "\n";
  piksi_check(can_set_bitrate(cInterfaceNameCan0, bitrate_can0),
              "Could not set bitrate %" PRId32 " on interface %s",
              bitrate_can0, cInterfaceNameCan0);
  piksi_check(can_set_bitrate(cInterfaceNameCan1, bitrate_can1),
              "Could not set bitrate %" PRId32 " on interface %s",
              bitrate_can1, cInterfaceNameCan1);

  // Set the controlmode to enable flexible datarate (FD) frames.
  // This is not supported yet in hardware, actually, but leave this commented
  // code here for potential future use.
  // This must be done before turning the interfaces on.
//   can_ctrlmode ctrlmode_fd_on = {CAN_CTRLMODE_FD, CAN_CTRLMODE_FD};
//   piksi_check(can_set_ctrlmode(cInterfaceNameCan0, &ctrlmode_fd_on),
//               "Could not enable FD frames on %s.", cInterfaceNameCan0);
//   piksi_check(can_set_ctrlmode(cInterfaceNameCan1, &ctrlmode_fd_on),
//               "Could not enable FD frames on %s.", cInterfaceNameCan1);

  // Turn the CAN interfaces on.
  piksi_check(can_do_start(cInterfaceNameCan0),
              "Could not start interface %s", cInterfaceNameCan0);
  piksi_check(can_do_start(cInterfaceNameCan1),
              "Could not start interface %s", cInterfaceNameCan1);

  // Open CAN sockets.
  int socket_can0 = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  int socket_can1 = socket(PF_CAN, SOCK_RAW, CAN_RAW);
  piksi_check(socket_can0 < 0, "Could not open a socket for CAN0.");
  piksi_check(socket_can1 < 0, "Could not open a socket for CAN1.");

  // Get indices of the CAN interfaces.
  int interface_can0_index = if_nametoindex(cInterfaceNameCan0);
  piksi_check(interface_can0_index < 0,
              "Could not get index of interface %s.", cInterfaceNameCan0);
  int interface_can1_index = if_nametoindex(cInterfaceNameCan1);
  piksi_check(interface_can1_index < 0,
              "Could not get index of interface %s.", cInterfaceNameCan1);

  // Enable reception of CAN flexible datarate (FD) frames.
  // Does nothing at the moment, actually, due to FD not being supported
  // in hardware. Does not hurt the have it here, nonetheless.
  piksi_check(setsockopt(socket_can0, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                         &cEnable, sizeof(cEnable)),
              "Could not enable reception of FD frames on %s.",
              cInterfaceNameCan0);
  piksi_check(setsockopt(socket_can1, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                         &cEnable, sizeof(cEnable)),
              "Could not enable reception of FD frames on %s.",
              cInterfaceNameCan1);

  // Disable loopback on CAN interfaces, so that messages are not received on
  // other programs using the same interfaces.
  piksi_check(setsockopt(socket_can0, SOL_CAN_RAW, CAN_RAW_LOOPBACK,
                         &loopback, sizeof(loopback)),
              "Could not disable loopback on %s.", cInterfaceNameCan0);
  piksi_check(setsockopt(socket_can1, SOL_CAN_RAW, CAN_RAW_LOOPBACK,
                         &loopback, sizeof(loopback)),
              "Could not disable loopback on %s.", cInterfaceNameCan1);

  // Disable receiving own messages on CAN sockets, so that messages are not
  // received on the same socket they are sent from.
  piksi_check(setsockopt(socket_can0, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
                         &loopback, sizeof(loopback)),
              "Could not disable reception of own messages on %s.",
              cInterfaceNameCan0);
  piksi_check(setsockopt(socket_can1, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS,
                         &loopback, sizeof(loopback)),
              "Could not disable reception of own messages on %s.",
              cInterfaceNameCan1);

  // Bind sockets to the network interfaces.
  sockaddr_can addr_can0;
  sockaddr_can addr_can1;
  addr_can0.can_family = AF_CAN;
  addr_can1.can_family = AF_CAN;
  addr_can0.can_ifindex = interface_can0_index;
  addr_can1.can_ifindex = interface_can1_index;
  piksi_check(bind(socket_can0, reinterpret_cast<sockaddr*>(&addr_can0),
                   sizeof(addr_can0)),
              "Could not bind to %s.", cInterfaceNameCan0);
  piksi_check(bind(socket_can1, reinterpret_cast<sockaddr*>(&addr_can1),
                   sizeof(addr_can1)),
              "Could not bind to %s.", cInterfaceNameCan1);

  // Prevent czmq from catching signals.
  zsys_handler_set(nullptr);

  // Create a pubsub for the can_sbp_bridge.
  sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(cEndpointPub, cEndpointSub);
  piksi_check(ctx == nullptr, "Could not create a pubsub.");

  // Init the pubsub.
  piksi_check(sbp_init(sbp_zmq_pubsub_rx_ctx_get(ctx),
                       sbp_zmq_pubsub_tx_ctx_get(ctx)),
              "Error initializing the pubsub.");

  // Put the CAN sockets into ZMQ pollitems.
  zmq_pollitem_t pollitem_can0 = {};
  zmq_pollitem_t pollitem_can1 = {};
  if(debug) {
    pollitem_can0.events = ZMQ_POLLIN;
    pollitem_can1.events = ZMQ_POLLIN;
    pollitem_can0.fd = socket_can0;
    pollitem_can1.fd = socket_can1;

    // Add CAN pollers to the zloop.
    zloop_poller(sbp_zmq_pubsub_zloop_get(ctx), &pollitem_can0,
                 callback_can_debug, const_cast<char *>(cInterfaceNameCan0));
    zloop_poller(sbp_zmq_pubsub_zloop_get(ctx), &pollitem_can1,
                 callback_can_debug, const_cast<char *>(cInterfaceNameCan1));
  }

  // Register callbacks for SBP messages.
  piksi_check(sbp_callback_register(SBP_MSG_TRACKING_STATE,
                                    callback_sbp_tracking_state,
                                    sbp_zmq_pubsub_zloop_get(ctx)),
              "Could not register callback. Message: %" PRIu16,
              SBP_MSG_TRACKING_STATE);
  piksi_check(sbp_callback_register(SBP_MSG_UTC_TIME, callback_sbp_utc_time,
                                    sbp_zmq_pubsub_zloop_get(ctx)),
              "Could not register callback. Message: %" PRIu16,
              SBP_MSG_UTC_TIME);
  piksi_check(sbp_callback_register(SBP_MSG_BASELINE_HEADING,
                                    callback_sbp_baseline_heading,
                                    sbp_zmq_pubsub_zloop_get(ctx)),
              "Could not register callback. Message: %" PRIu16,
              SBP_MSG_BASELINE_HEADING);
  piksi_check(sbp_callback_register(SBP_MSG_POS_LLH, callback_sbp_pos_llh,
                                    sbp_zmq_pubsub_zloop_get(ctx)),
              "Could not register callback. Message: %" PRIu16,
              SBP_MSG_POS_LLH);
  piksi_check(sbp_callback_register(SBP_MSG_VEL_NED, callback_sbp_vel_ned,
                                    sbp_zmq_pubsub_zloop_get(ctx)),
              "Could not register callback. Message: %" PRIu16,
              SBP_MSG_VEL_NED);
  piksi_check(sbp_callback_register(SBP_MSG_DOPS, callback_sbp_dops,
                                    sbp_zmq_pubsub_zloop_get(ctx)),
              "Could not register callback. Message: %" PRIu16,
              SBP_MSG_DOPS);
  piksi_check(sbp_callback_register(SBP_MSG_HEARTBEAT, callback_sbp_heartbeat,
                                    sbp_zmq_pubsub_zloop_get(ctx)),
              "Could not register callback. Message: %" PRIu16,
              SBP_MSG_HEARTBEAT);

  // Read the serial number.
  { // The block automagically cleans up the file object.
    std::fstream serial_num_file(cModelSerialCodePath, std::ios_base::in);
    serial_num_file.read(manufacturers_model_serial_code,
                         sizeof(manufacturers_model_serial_code) - 1);
  }

  // Set N2K info and options.
  NMEA2000.SetN2kCANSendFrameBufSize(32);
  // TODO(lstrz): Need an elegant way to query for the proper info to include here.
  // https://github.com/swift-nav/firmware_team_planning/issues/452
  // ModelSerialCode, ProductCode, ModelID, SwCode, ModelVersion,
  // LoadEquivalency, N2kVersion, CertificationLevel, UniqueNumber,
  // DeviceFunction, DeviceClass, ManufacturerCode, IndustryGroup
  NMEA2000.SetProductInformation(manufacturers_model_serial_code,
                                 cNmeaManufacturersProductCode, 0, 0,
                                 0, cLoadEquivalency,
                                 cNmeaNetworkMessageDatabaseVersion,
                                 cNMEA2000CertificationLevel);
  NMEA2000.SetDeviceInformation(0x1337,
          /*_DeviceFunction=*/0xff,
          /*_DeviceClass=*/0xff, /*_ManufacturerCode=*/883);
  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode);
  NMEA2000.SetHeartbeatInterval(1000);
  NMEA2000.ExtendTransmitMessages(cTransmitPGNs);
  piksi_check(!dynamic_cast<tNMEA2000_SocketCAN&>(NMEA2000).CANOpenForReal(socket_can0),
              "Could not open N2k for real.");
  NMEA2000.Open();
  NMEA2000.SendIsoAddressClaim();
  NMEA2000.ParseMessages();

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

  exit(EXIT_SUCCESS);
}
