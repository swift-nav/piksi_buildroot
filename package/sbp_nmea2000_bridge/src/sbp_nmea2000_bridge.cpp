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
#include <libsbp/can.h>
#include <libsbp/navigation.h>
#include <libsbp/orientation.h>
#include <libsbp/sbp.h>
#include <libsbp/system.h>
#include <libsbp/tracking.h>
}

#define USE_N2K_CAN 5
#include <N2kMessages.h>
#include <libpiksi/common.h>
#include "NMEA2000_CAN.h"
#include "sbp.h"

#define UNUSED(x) (void)(x)

namespace {
    constexpr char cProgramName[] = "sbp_nmea2000_bridge";

    bool debug = false;

    constexpr char cInterfaceNameCan0[] = "can0";
    constexpr char cInterfaceNameCan1[] = "can1";

    u32 bitrate_can0 = 500 * 1000;
    u32 bitrate_can1 = 500 * 1000;

    // Socket where this process will take in SBP messages to be put on the CAN.
    constexpr char cEndpointSub[] = ">tcp://127.0.0.1:43090";
    // Socket where this process will output CAN wrapped in SBP.
    // Not used at present, actually.
    constexpr char cEndpointPub[] = ">tcp://127.0.0.1:43091";

    constexpr char cSerialNumberPath[] = "/factory/mfg_id";

    // Maps time of week to a sequence ID in a circular buffer.
    // Assumes that time of week monotonously increases and that every
    // time of week value will go through this map. In other words, if values
    // (increasing order) tow1, tow2, tow4 are already mapped, the value tow3
    // will not fit in the  map anymore.
    class TowSequenceIdMap {
    public:
        u8 LatestSequenceId() {
          return sequence_id_;
        }

        u8 operator[](u32 tow) {
          if(tow > buffer_[head_].first) {  // Add new tow->sid mapping.
            IncrementHead();
            IncrementSequenceId();
            buffer_[head_].first = tow;
            buffer_[head_].second = sequence_id_;
            return sequence_id_;
          }

          if(tow < buffer_[GetOldestEntryIndex()].first) {  // Tow too old.
            return 0xFF;
          }

          // Look up tow in the map.
          // The map is small enough for linear search to be ok-ish.
          for(auto p : buffer_) {
            if(p.first == tow) {
              return p.second;
            }
          }

          return 0xFF;  // Tow not found.
        }
    private:
        void IncrementHead() {
          ++head_;
          if(head_ >= buffer_.size()){
            head_ = 0;
          }
        }

        u8 GetOldestEntryIndex() {
          u8 oldest_entry_index = head_ + 1;
          if(oldest_entry_index >= buffer_.size()){
            oldest_entry_index = 0;
          }
          return oldest_entry_index;
        }

        u8 IncrementSequenceId() {
          ++sequence_id_;
          if(sequence_id_ > 252){
            sequence_id_ = 0;
          }
          return sequence_id_;
        }

        // TODO(lstrz): Determine if a smaller buffer size would be enough.
        static constexpr u32 cSequenceIdBufferSize = 16;
        std::array<std::pair<u32, u8>, cSequenceIdBufferSize> buffer_ = {};
        u8 head_ = 0;
        u8 sequence_id_ = 0;
    };
    TowSequenceIdMap tow_sequence_id_map;

    // Cache some info for composite N2k messages.
    class N2kCompositeMessageCache {
    public:
        N2kCompositeMessageCache() {
          // Pass a 1, as the smallest integer larger than 0, to initialize
          // the bitfields to zero.
          ResetIfOld(1);
        }

        void ResetIfOld(u32 tow) {
          if(tow > tow_) {
            tow_ = tow;
            seconds_since_midnight_set_ = lat_set_ = lon_set_ = alt_set_ =
            hdop_set_ = pdop_set_ = tow_set_ = days_since_1970_set_ =
            gnss_metod_set_ = sat_count_set_ = ref_station_count_set_ = false;
          }
        }

        bool IsAllFieldsSet() {
          return seconds_since_midnight_set_ && lat_set_ && lon_set_ &&
                 alt_set_ && hdop_set_ && pdop_set_ && tow_set_ &&
                 days_since_1970_set_ && gnss_metod_set_ &&
                 sat_count_set_ && ref_station_count_set_;
        }

        void SetFields(const u16 days_since_1970,
                       const double seconds_since_midnight) {
          days_since_1970_ = days_since_1970;
          seconds_since_midnight_ = seconds_since_midnight;
          days_since_1970_set_ = seconds_since_midnight_set_ = true;
        }

        void SetFields(const msg_pos_llh_t *msg) {
          lat_ = msg->lat;
          lon_ = msg->lon;
          alt_ = msg->height;
          sat_count_ = msg->n_sats;
          gnss_metod_ = gnss_method_lut_[msg->flags];
          lat_set_ = lon_set_ = alt_set_ = sat_count_set_ = gnss_metod_set_ =
                  true;
        }

        void SetFields(const msg_dops_t *msg) {
          hdop_ = msg->hdop;
          pdop_ = msg->pdop;
          hdop_set_ = pdop_set_ = true;
        }

        // PGN 129029
        void FillN2kMsg(tN2kMsg *msg) {
          SetN2kGNSS(*msg, tow_sequence_id_map[tow_], days_since_1970_,
                     seconds_since_midnight_, lat_, lon_, alt_,
                     tN2kGNSStype::N2kGNSSt_GPSGLONASS,
                     tN2kGNSSmethod::N2kGNSSm_PreciseGNSS, sat_count_, hdop_,
                     pdop_);
          // Have to do GNSS method manually. It's not reverse engineered yet.
          msg->Data[31] = gnss_metod_;
        }
    private:
        // Maps SBP GNSS method code to N2K GNSS method code.
        // 0xX2 is GPS + GLONASS type of system.
        static constexpr u8 gnss_method_lut_[] = {0x02, 0x12, 0x22, 0x52, 0x42};

        double seconds_since_midnight_ = 0;
        double lat_ = 0;
        double lon_ = 0;
        double alt_ = 0;
        double hdop_ = 0;
        double pdop_ = 0;
        u32 tow_ = 0;
        uint16_t days_since_1970_ = 0;
        u8 gnss_metod_ = 0;
        u8 sat_count_ = 0;
        u8 ref_station_count_ = 0;

        bool seconds_since_midnight_set_ : 1;
        bool lat_set_ : 1;
        bool lon_set_ : 1;
        bool alt_set_ : 1;
        bool hdop_set_ : 1;
        bool pdop_set_ : 1;
        bool tow_set_ : 1;
        bool days_since_1970_set_ : 1;
        bool gnss_metod_set_ : 1;
        bool sat_count_set_ : 1;
        bool ref_station_count_set_ : 1;
    };
    N2kCompositeMessageCache n2k_composite_message_cache;
    constexpr u8 N2kCompositeMessageCache::gnss_method_lut_[];

    void usage(char *command) {
      std::cout << "Usage: " << command << " [--bitrate N]" << std::endl
                << "Where N is an acceptable CAN bitrate." << std::endl;

      std::cout << std::endl << "Misc options:" << std::endl;
      std::cout << "\t--debug" << std::endl;
    }

    int parse_options(int argc, char *argv[]) {
      enum {
          OPT_ID_DEBUG = 0,
          OPT_ID_BITRATE = 1,
      };

      constexpr option long_opts[] = {
              {"debug",   no_argument,       nullptr, OPT_ID_DEBUG},
              {"bitrate", required_argument, nullptr, OPT_ID_BITRATE},
              {nullptr,   no_argument,       nullptr, 0}
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
      /* Millimeters to meters. */
      double vel_north_mps = sbp_vel_ned->n / 1000.0;
      double vel_east_mps = sbp_vel_ned->e / 1000.0;

      *cog_rad = atan2(vel_east_mps, vel_north_mps);

      /* Convert negative values to positive */
      constexpr double kFullCircleRad = 2 * 3.1415926535897932384626433832795;
      if (*cog_rad < 0.0) {
        *cog_rad += kFullCircleRad;
      }

      /* Avoid having duplicate values for same point (0 and 360) */
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
      tm_utc_time.tm_isdst = -1;  // No idea. mktime() will know.
      time_t utc_time_since_epoch = mktime(&tm_utc_time);
      *days_since_1970 =
              static_cast<u16>(utc_time_since_epoch / (3600 * 24));
      *seconds_since_midnight =
              utc_time_since_epoch % (3600 * 24) + sbp_utc_t->ns * 10e-9;
    }

    void piksi_check(int err, const char* format, ...) {
      if (err != 0) {
        va_list ap;
        va_start(ap, format);
        piksi_log(LOG_ERR, format, ap);
        va_end(ap);
        exit(EXIT_FAILURE);
      }
    }

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
      u16 days_since_1970;
      double seconds_since_midnight;
      calculate_days_and_seconds_since_1970(sbp_utc_time,
                                            &days_since_1970,
                                            &seconds_since_midnight);

      tN2kMsg N2kMsg;
      SetN2kSystemTime(N2kMsg, tow_sequence_id_map[sbp_utc_time->tow],
                       days_since_1970, seconds_since_midnight,
                       tN2kTimeSource::N2ktimes_GPS);
      NMEA2000.SendMsg(N2kMsg);

      n2k_composite_message_cache.ResetIfOld(sbp_utc_time->tow);
      n2k_composite_message_cache.SetFields(days_since_1970,
                                            seconds_since_midnight);
      if(n2k_composite_message_cache.IsAllFieldsSet()){
        N2kMsg.Clear();
        n2k_composite_message_cache.FillN2kMsg(&N2kMsg);
        NMEA2000.SendMsg(N2kMsg);
      }
    }

    // PGN 127250
    void callback_sbp_baseline_heading(u16 sender_id, u8 len, u8 msg[], void *context){
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(context);

      auto sbp_baseline_heading = reinterpret_cast<msg_baseline_heading_t*>(msg);

      tN2kMsg N2kMsg;
      SetN2kTrueHeading(N2kMsg, tow_sequence_id_map[sbp_baseline_heading->tow],
                        DegToRad(sbp_baseline_heading->heading) / 1000.0);
      NMEA2000.SendMsg(N2kMsg);
    }

    // PGN 129025
    void callback_sbp_pos_llh(u16 sender_id, u8 len, u8 msg[], void *context) {
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(context);

      auto *sbp_pos = reinterpret_cast<msg_pos_llh_t*>(msg);

      tN2kMsg N2kMsg;
      SetN2kLatLonRapid(N2kMsg, sbp_pos->lat, sbp_pos->lon);
      NMEA2000.SendMsg(N2kMsg);

      n2k_composite_message_cache.ResetIfOld(sbp_pos->tow);
      n2k_composite_message_cache.SetFields(sbp_pos);
      if(n2k_composite_message_cache.IsAllFieldsSet()){
        N2kMsg.Clear();
        n2k_composite_message_cache.FillN2kMsg(&N2kMsg);
        NMEA2000.SendMsg(N2kMsg);
      }
    }

    // PGN 129026
    void callback_sbp_vel_ned(u16 sender_id, u8 len, u8 msg[], void *context){
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(context);

      auto *sbp_vel_ned = reinterpret_cast<msg_vel_ned_t*>(msg);
      double cog_rad, sog_mps;
      calculate_cog_sog(sbp_vel_ned, &cog_rad, &sog_mps);

      tN2kMsg N2kMsg;
      SetN2kCOGSOGRapid(N2kMsg, tow_sequence_id_map[sbp_vel_ned->tow],
                        tN2kHeadingReference::N2khr_true,
                        cog_rad, sog_mps);
      NMEA2000.SendMsg(N2kMsg);
    }

    // PGN 129539
    void callback_sbp_dops(u16 sender_id, u8 len, u8 msg[], void *context){
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(context);

      auto sbp_dops = reinterpret_cast<msg_dops_t*>(msg);

      tN2kMsg N2kMsg;
      SetN2kGNSSDOPData(N2kMsg, tow_sequence_id_map[sbp_dops->tow],
              /*DesiredMode=*/tN2kGNSSDOPmode::N2kGNSSdm_Auto,
              /*ActualMode=*/(sbp_dops->flags == 0) ?
                             tN2kGNSSDOPmode::N2kGNSSdm_Error :
                             tN2kGNSSDOPmode::N2kGNSSdm_3D,
                        sbp_dops->hdop / 100.0,
                        sbp_dops->vdop / 100.0,
                        sbp_dops->tdop / 100.0);
      NMEA2000.SendMsg(N2kMsg);

      n2k_composite_message_cache.ResetIfOld(sbp_dops->tow);
      n2k_composite_message_cache.SetFields(sbp_dops);
      if(n2k_composite_message_cache.IsAllFieldsSet()){
        N2kMsg.Clear();
        n2k_composite_message_cache.FillN2kMsg(&N2kMsg);
        NMEA2000.SendMsg(N2kMsg);
      }
    }

    // PGN 129540
    void callback_sbp_tracking_state(u16 sender_id, u8 len, u8 msg[],
                                     void *context) {
      UNUSED(sender_id);
      UNUSED(context);
      d << "callback_sbp_tracking_state\n"
        << "\tGot " << static_cast<u16>(len) << "/"
        << sizeof(tracking_channel_state_t) << "="
        << static_cast<u16>(len / sizeof(tracking_channel_state_t))
        << " states. Message len is most likely statically allocated.\n";

      constexpr u8 cConstellationMaxSats = 64;
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

        switch(tracking_channel_state.sid.code) {
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
          return a.first < b.first;
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
      d << "\tDeduplicated. " << sats_gps_end_it - sats_gps.begin() +
                                 sats_glo_end_it - sats_glo.begin()
        << " sats left. GPS: " << sats_gps_end_it - sats_gps.begin()
        << " GLONASS: " << sats_glo_end_it - sats_glo.begin() << "\n";

      tN2kMsg N2kMsg;
      N2kMsg.SetPGN(129540L);
      N2kMsg.Priority=6;
      // This sequence ID is a guess. Have no info to get a definite one.
      N2kMsg.AddByte(tow_sequence_id_map.LatestSequenceId());
      // TODO(lstrz): Can I get range residuals from someplace?
      N2kMsg.AddByte(0xFF);
      N2kMsg.AddByte(sats_gps_end_it - sats_gps.begin() +
                     sats_glo_end_it - sats_glo.begin());
      for(auto it = sats_gps.begin(); it != sats_gps_end_it; ++it) {
        N2kMsg.AddByte(/*PRN=*/it->first);
        // TODO(lstrz): Can I get elevation and azimuth from someplace?
        // Stefan: not in SBP, but see NMEA 183 message GSV implementation,
        // we have (should have) some there I think
        N2kMsg.Add2ByteUDouble(/*elevation=*/N2kDoubleNA, 0.0001, N2kDoubleNA);
        N2kMsg.Add2ByteUDouble(/*azimuth=*/N2kDoubleNA, 0.0001, N2kDoubleNA);
        N2kMsg.Add2ByteUDouble(/*CN0=*/it->second * 0.25, 0.01, N2kDoubleNA);
        N2kMsg.Add4ByteUInt(/*rangeResiduals=*/N2kInt32NA);
        // TODO(lstrz): Is the satellite used? Can I get that info someplece?
        N2kMsg.AddByte(/*tracked=*/0xFF);
      }
      for(auto it = sats_glo.begin(); it != sats_glo_end_it; ++it) {
        N2kMsg.AddByte(/*PRN=*/it->first + 64);
        // TODO(lstrz): Can I get elevation and azimuth from someplace?
        // Stefan: not in SBP, but see NMEA 183 message GSV implementation,
        // we have (should have) some there I think
        N2kMsg.Add2ByteUDouble(/*elevation=*/N2kDoubleNA, 0.0001, N2kDoubleNA);
        N2kMsg.Add2ByteUDouble(/*azimuth=*/N2kDoubleNA, 0.0001, N2kDoubleNA);
        N2kMsg.Add2ByteUDouble(/*CN0=*/it->second * 0.25, 0.01, N2kDoubleNA);
        N2kMsg.Add4ByteUInt(/*rangeResiduals=*/N2kInt32NA);
        // TODO(lstrz): Is the satellite used? Can I get that info someplece?
        N2kMsg.AddByte(/*tracked=*/0xFF);
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

      // This checks for new CAN messages and sends out a heartbeat.
      NMEA2000.ParseMessages();
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
  printf("Setting bitrate to %" PRIu32 " and %" PRIu32 ".\n",
         bitrate_can0, bitrate_can1);
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
  int enable = 1;
  piksi_check(setsockopt(socket_can0, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                         &enable, sizeof(enable)),
              "Could not enable reception of FD frames on %s.",
              cInterfaceNameCan0);
  piksi_check(setsockopt(socket_can1, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                         &enable, sizeof(enable)),
              "Could not enable reception of FD frames on %s.",
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
  std::string serial_num_str;
  unsigned long serial_num;
  { // The block automagically cleans up the file object.
    std::fstream serial_num_file(cSerialNumberPath, std::ios_base::in);
    serial_num_file >> serial_num_str;
    std::stringstream ss;
    ss.str(serial_num_str);
    ss >> serial_num;
  }

  // Set N2K info and options.
  NMEA2000.SetN2kCANSendFrameBufSize(32);
  // TODO(lstrz): Need an elegant way to query for the proper info to include here.
  // https://github.com/swift-nav/firmware_team_planning/issues/452
  // ModelSerialCode, ProductCode, ModelID, SwCode, ModelVersion,
  // LoadEquivalency, N2kVersion, CertificationLevel, UniqueNumber,
  // DeviceFunction, DeviceClass, ManufacturerCode, IndustryGroup
  NMEA2000.SetProductInformation(serial_num_str.c_str());
  NMEA2000.SetDeviceInformation(serial_num, /*_DeviceFunction=*/0xff,
          /*_DeviceClass=*/0xff, /*_ManufacturerCode=*/883);
  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode);
  NMEA2000.Open();
  NMEA2000.ParseMessages();

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

  exit(EXIT_SUCCESS);
}
