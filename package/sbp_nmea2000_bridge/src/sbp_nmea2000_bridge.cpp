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

#include <czmq.h>
#include <fstream>
#include <getopt.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/sockios.h>
#include <libsocketcan.h>
#include <net/if.h>
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
#include <libsbp/system.h>
#include <libsbp/tracking.h>
}

#include "callbacks.h"
#include "common.h"
#include "device_and_product_info.h"
#include "NMEA2000_SocketCAN.h"

namespace {
    constexpr char cProgramName[] = "sbp_nmea2000_bridge";

    int loopback = false;
    bool n2k_tracking = true;
    bool n2k_utc = true;
    bool n2k_heading = true;
    bool n2k_llh = true;
    bool n2k_vel_ned = true;
    bool n2k_dops = true;
    bool n2k_heartbeat = true;

    constexpr char cInterfaceNameCan0[] = "can0";
    constexpr char cInterfaceNameCan1[] = "can1";

    // Terminate with a zero.
    constexpr unsigned long cTransmitPGNsAll[] = {126992, 127250, 129025,
                                                  129026, 129029, 129539,
                                                  129540, 0};
    constexpr unsigned long cTransmitPGNsBase[] = {126992, 129025, 129026,
                                                   129029, 129539, 129540, 0};
    constexpr unsigned long cTransmitPGNsRover[] = {127250, 0};
    const unsigned long *transmit_pgns = cTransmitPGNsAll;

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
    char manufacturers_model_id[32] = "";  // Piksi Multi or Piksi Duro or Piksi Nano.
    char manufacturers_software_version_code[32] = "";
    constexpr char cManufacturersModelVersion[32] = "Rev. X";
    char manufacturers_model_serial_code[32] = "";
    // cNMEA2000CertificationLevel is not applicable any more per standard.
    // Therefore, a value of 2 is used.
    constexpr u8 cNMEA2000CertificationLevel = 2;
    constexpr u8 cLoadEquivalency = 8;

    // Device information.
    u32 unique_number = 0;
    constexpr u16 cManufacturerCode = 883;
    constexpr u8 cDeviceFunction = 145;  // Ownship position (GNSS).
    constexpr u8 cDeviceClass = 60;  // Nvigation.
    constexpr u8 cIndustryGroup = 0;  // Global.

    void usage(char *command) {
      std::cout << "Usage: " << command << " [--bitrate N] [--debug] [--loopback]" << std::endl
                << "Where N is an acceptable CAN bitrate." << std::endl;

      std::cout << std::endl << "Misc options:" << std::endl;
      std::cout << "\t--debug" << std::endl;
    }

    int parse_options(int argc, char *argv[]) {
      enum {
          OPT_ID_DEBUG = 0,
          OPT_ID_BITRATE,
          OPT_ID_LOOPBACK,
          OPT_ID_NO_TRACKING,
          OPT_ID_NO_UTC,
          OPT_ID_NO_HEADING,
          OPT_ID_NO_LLH,
          OPT_ID_NO_VEL_NED,
          OPT_ID_NO_DOPS,
          OPT_ID_NO_HEARTBEAT,
          OPT_ID_ROVER,
          OPT_ID_BASE,
      };

      constexpr option long_opts[] = {
              {"debug",        no_argument,       nullptr, OPT_ID_DEBUG},
              {"bitrate",      required_argument, nullptr, OPT_ID_BITRATE},
              {"loopback",     no_argument,       nullptr, OPT_ID_LOOPBACK},
              {"no-tracking",  no_argument,       nullptr, OPT_ID_NO_TRACKING},
              {"no-utc",       no_argument,       nullptr, OPT_ID_NO_UTC},
              {"no-heading",   no_argument,       nullptr, OPT_ID_NO_HEADING},
              {"no-llh",       no_argument,       nullptr, OPT_ID_NO_LLH},
              {"no-vel-ned",   no_argument,       nullptr, OPT_ID_NO_VEL_NED},
              {"no-dops",      no_argument,       nullptr, OPT_ID_NO_DOPS},
              {"no-heartbeat", no_argument,       nullptr, OPT_ID_NO_HEARTBEAT},
              {"rover",        no_argument,       nullptr, OPT_ID_ROVER},
              {"base",         no_argument,       nullptr, OPT_ID_BASE},
              {nullptr,        no_argument,       nullptr, 0}
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
          case OPT_ID_NO_TRACKING:
            n2k_tracking = false;
            break;
          case OPT_ID_NO_UTC:
            n2k_utc = false;
            break;
          case OPT_ID_NO_HEADING:
            n2k_heading = false;
            break;
          case OPT_ID_NO_LLH:
            n2k_llh = false;
            break;
          case OPT_ID_NO_VEL_NED:
            n2k_vel_ned = false;
            break;
          case OPT_ID_NO_DOPS:
            n2k_dops = false;
            break;
          case OPT_ID_NO_HEARTBEAT:
            n2k_heartbeat = false;
            break;
          case OPT_ID_ROVER:
            transmit_pgns = cTransmitPGNsRover;
            n2k_tracking = n2k_utc = n2k_llh = n2k_vel_ned = n2k_dops = false;
            break;
          case OPT_ID_BASE:
            transmit_pgns = cTransmitPGNsBase;
            n2k_heading = false;
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
  if (n2k_tracking) {
    piksi_check(sbp_callback_register(SBP_MSG_TRACKING_STATE,
                                      callback_sbp_tracking_state,
                                      sbp_zmq_pubsub_zloop_get(ctx)),
                "Could not register callback. Message: %" PRIu16,
                SBP_MSG_TRACKING_STATE);
  }
  if (n2k_utc) {
    piksi_check(sbp_callback_register(SBP_MSG_UTC_TIME, callback_sbp_utc_time,
                                      sbp_zmq_pubsub_zloop_get(ctx)),
                "Could not register callback. Message: %" PRIu16,
                SBP_MSG_UTC_TIME);
  }
  if (n2k_heading) {
    piksi_check(sbp_callback_register(SBP_MSG_BASELINE_HEADING,
                                      callback_sbp_baseline_heading,
                                      sbp_zmq_pubsub_zloop_get(ctx)),
                "Could not register callback. Message: %" PRIu16,
                SBP_MSG_BASELINE_HEADING);
  }
  if (n2k_llh) {
    piksi_check(sbp_callback_register(SBP_MSG_POS_LLH, callback_sbp_pos_llh,
                                      sbp_zmq_pubsub_zloop_get(ctx)),
                "Could not register callback. Message: %" PRIu16,
                SBP_MSG_POS_LLH);
  }
  if (n2k_vel_ned) {
    piksi_check(sbp_callback_register(SBP_MSG_VEL_NED, callback_sbp_vel_ned,
                                      sbp_zmq_pubsub_zloop_get(ctx)),
                "Could not register callback. Message: %" PRIu16,
                SBP_MSG_VEL_NED);
  }
  if (n2k_dops) {
    piksi_check(sbp_callback_register(SBP_MSG_DOPS, callback_sbp_dops,
                                      sbp_zmq_pubsub_zloop_get(ctx)),
                "Could not register callback. Message: %" PRIu16,
                SBP_MSG_DOPS);
  }
  if(n2k_heartbeat) {
    piksi_check(sbp_callback_register(SBP_MSG_HEARTBEAT, callback_sbp_heartbeat,
                                      sbp_zmq_pubsub_zloop_get(ctx)),
                "Could not register callback. Message: %" PRIu16,
                SBP_MSG_HEARTBEAT);
  }

  // Set N2K info and options.
  get_manufacturers_model_id(sizeof(manufacturers_model_id),
                             manufacturers_model_id);
  get_manufacturers_software_version_code(
          sizeof(manufacturers_software_version_code),
          manufacturers_software_version_code);
  get_manufacturers_model_serial_code(sizeof(manufacturers_model_serial_code),
                                      manufacturers_model_serial_code);
  // Modulo with max possible valid value for the unique number.
  unique_number = std::hash<std::string>{}(manufacturers_model_serial_code)
                  % ((1 << 20) - 1);

  NMEA2000.SetN2kCANSendFrameBufSize(32);

  cout << "Product Information:" << endl
       << "\tNMEA Network Message Database Version: "
       << cNmeaNetworkMessageDatabaseVersion << endl
       << "\tNMEA Manufacturer's Product Code: "
       << cNmeaManufacturersProductCode << endl
       << "\tManufacturer's Model ID: "
       << manufacturers_model_id << endl
       << "\tManufacturer's Software Version Code: "
       << manufacturers_software_version_code << endl
       << "\tManufacturer's Model Version: "
       << cManufacturersModelVersion << endl
       << "\tManufacturer's Model Serial Code: "
       << manufacturers_model_serial_code << endl
       << "\tNMEA2000 Certification Level: "
       << static_cast<u16>(cNMEA2000CertificationLevel) << endl
       << "\tLoad Equivalency: "
       << static_cast<u16>(cLoadEquivalency) << endl;

  cout << "Device Information:" << endl
       << "\tUnique Number: " << unique_number << endl
       << "\tManufacturer Code: " << cManufacturerCode << endl
       << "\tDevice Function: " << static_cast<u16>(cDeviceFunction) << endl
       << "\tDevice Class: " << static_cast<u16>(cDeviceClass) << endl
       << "\tIndustry Group: " << static_cast<u16>(cIndustryGroup) << endl;

  NMEA2000.SetProductInformation(manufacturers_model_serial_code,
                                 cNmeaManufacturersProductCode,
                                 manufacturers_model_id,
                                 manufacturers_software_version_code,
                                 cManufacturersModelVersion, cLoadEquivalency,
                                 cNmeaNetworkMessageDatabaseVersion,
                                 cNMEA2000CertificationLevel);
  NMEA2000.SetDeviceInformation(unique_number, cDeviceFunction, cDeviceClass,
                                cManufacturerCode, cIndustryGroup);
  NMEA2000.SetMode(tNMEA2000::N2km_ListenAndNode);
  NMEA2000.SetHeartbeatInterval(1000);
  NMEA2000.ExtendTransmitMessages(transmit_pgns);
  piksi_check(!dynamic_cast<tNMEA2000_SocketCAN&>(NMEA2000).CANOpenForReal(socket_can0),
              "Could not open N2k for real.");

  while (zmq_simple_loop_timeout(sbp_zmq_pubsub_zloop_get(ctx), 50) != -1) {
    NMEA2000.ParseMessages();
  }

  exit(EXIT_SUCCESS);
}
