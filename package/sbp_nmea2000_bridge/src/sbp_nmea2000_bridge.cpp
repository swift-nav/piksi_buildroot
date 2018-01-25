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
#include <vector>

extern "C" {
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/util.h>
#include <libpiksi/logging.h>
#include <libsbp/can.h>
#include <libsbp/navigation.h>
#include <libsbp/sbp.h>
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

    class SequenceId{
    public:
        static u8 Get() {
          return sequence_id_;
        }

        static u8 Next() {
          ++sequence_id_;
          if(sequence_id_ > 252){
            sequence_id_ = 0;
          }
          return sequence_id_;
        }
    private:
        static u8 sequence_id_;
    };
    u8 SequenceId::sequence_id_ = 0;

    // Cache current time for N2k.
    struct TimeForN2k{
        uint16_t DaysSince1970;
        double SecondsSinceMidnight;
    } TimeForN2k;

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

    void piksi_check(int err, const char* format, ...) {
      if (err != 0) {
        va_list ap;
        va_start(ap, format);
        piksi_log(LOG_ERR, format, ap);
        if(debug){
          printf(format, ap);
          printf("\n");
        }
        va_end(ap);
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
                  << frame.data[i];
      }
      std::cout << std::endl;

      return 0;
    }

    void callback_sbp_utc_time(u16 sender_id, u8 len, u8 msg[], void *context) {
      UNUSED(sender_id);
      UNUSED(len);
      UNUSED(context);

      auto sbp_utc_time = reinterpret_cast<msg_utc_time_t*>(msg);
      tm tm_utc_time;
      // 1900 is the starting year (year 0) in tm_year, so subtract 1900.
      tm_utc_time.tm_year = sbp_utc_time->year - 1900;
      // 0 is the starting month in tm_mon, so subtract 1.
      tm_utc_time.tm_mon = sbp_utc_time->month - 1;
      tm_utc_time.tm_mday = sbp_utc_time->day;
      tm_utc_time.tm_hour = sbp_utc_time->hours;
      tm_utc_time.tm_min = sbp_utc_time->minutes;
      tm_utc_time.tm_sec = sbp_utc_time->seconds;
      tm_utc_time.tm_isdst = -1;  // No idea. mktime() will know.
      time_t utc_time_since_epoch = mktime(&tm_utc_time);
      TimeForN2k.DaysSince1970 =
              static_cast<u16>(utc_time_since_epoch / (3600 * 24));
      TimeForN2k.SecondsSinceMidnight =
              utc_time_since_epoch % (3600 * 24) + sbp_utc_time->ns * 10e-9;

      tN2kMsg N2kMsg;
      SetN2kSystemTime(N2kMsg, SequenceId::Next(), TimeForN2k.DaysSince1970,
                       TimeForN2k.SecondsSinceMidnight,
                       tN2kTimeSource::N2ktimes_GPS);
      NMEA2000.SendMsg(N2kMsg);
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
  piksi_check(sbp_callback_register(SBP_MSG_UTC_TIME, callback_sbp_utc_time,
                                    sbp_zmq_pubsub_zloop_get(ctx)),
              "Could not register callback. Message: %" PRIu16, SBP_MSG_UTC_TIME);

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
