/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

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
#include <libpiksi/settings.h>
#include <libpiksi/util.h>
#include <libpiksi/logging.h>
#include <libsbp/navigation.h>
#include <libsbp/system.h>
#include <libsbp/tracking.h>
}

#include "callbacks.h"
#include "common.h"
#include "device_and_product_info.h"
#include "NMEA2000_SocketCAN.h"
#include "sbp.h"

#define PROGRAM_NAME "sbp_nmea2000_bridge"

#define NMEA2000_SUB_ENDPOINT "ipc:///var/run/sockets/nmea2000_internal.pub" /* NMEA2000 Internal Out */
#define NMEA2000_PUB_ENDPOINT "ipc:///var/run/sockets/nmea2000_internal.sub" /* NMEA2000 Internal In */

namespace {
    // constexpr char cProgramName[] = "sbp_nmea2000_bridge";

    int loopback = false;

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

    pk_endpoint_t *nmea2000_pub = nullptr;

    // // Socket where this process will take in SBP messages to be put on the CAN.
    // constexpr char cEndpointSub[] = ">tcp://127.0.0.1:43090";
    // // Socket where this process will output CAN wrapped in SBP.
    // // Not used at present, actually.
    // constexpr char cEndpointPub[] = ">tcp://127.0.0.1:43091";

    // Product information.
    constexpr char cSettingsCategoryName[] = "nmea2000";
    constexpr u16 cNmeaNetworkMessageDatabaseVersion = 2100;
    constexpr u16 cNmeaManufacturersProductCode = 0xFFFF;  // Assigned by NMEA.
    char manufacturers_model_id[32] = "PIKSI MULTI";  // Default to Piksi Multi.
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
          OPT_ID_ROVER,
          OPT_ID_BASE,
      };

      constexpr option long_opts[] = {
              {"debug",        no_argument,       nullptr, OPT_ID_DEBUG},
              {"bitrate",      required_argument, nullptr, OPT_ID_BITRATE},
              {"loopback",     no_argument,       nullptr, OPT_ID_LOOPBACK},
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
          case OPT_ID_ROVER:
            transmit_pgns = cTransmitPGNsRover;
            setting_sbp_tracking = setting_sbp_utc = setting_sbp_llh = setting_sbp_vel_ned = setting_sbp_dops = false;
            break;
          case OPT_ID_BASE:
            transmit_pgns = cTransmitPGNsBase;
            setting_sbp_heading = false;
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

struct nmea2000_sbp_state {

};
struct nmea2000_sbp_state nmea2000_to_sbp_state;

static int nmea2sbp_decode_frame(const uint8_t *frame,
                                  uint32_t frame_length,
                                  void *state) {
  UNUSED(frame);
  UNUSED(frame_length);
  UNUSED(state);
  return 0;
}

static void nmea2000_reader_handler(pk_loop_t *loop, void *handle, void *context)
{
  (void)loop;
  (void)handle;
  pk_endpoint_t *nmea2000_ept_loc = (pk_endpoint_t *)context;
  if (pk_endpoint_receive(nmea2000_ept_loc, nmea2sbp_decode_frame, &nmea2000_to_sbp_state) != 0) {
    piksi_log(LOG_ERR,
              "%s: error in %s (%s:%d): %s",
              __FUNCTION__,
              "pk_endpoint_receive",
              __FILE__,
              __LINE__,
              pk_endpoint_strerror());
  }
}

static int cleanup(pk_endpoint_t **nmea2000_ept_loc, int status);

int main(int argc, char *argv[]) {
  settings_ctx_t *settings_ctx = nullptr;
  pk_loop_t *loop = nullptr;
  pk_endpoint_t *nmea2000_sub = nullptr;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "Invalid arguments.");
    usage(argv[0]);
    exit(cleanup(&nmea2000_sub, EXIT_FAILURE));
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

  if (sbp_init() != 0) {
    piksi_log(LOG_ERR, "error initializing SBP");
    exit(cleanup(&nmea2000_sub, EXIT_FAILURE));
  }

  // Create a pubsub for the can_sbp_bridge.
  nmea2000_pub = pk_endpoint_create(NMEA2000_PUB_ENDPOINT, PK_ENDPOINT_PUB);
  if (nmea2000_pub == nullptr) {
    piksi_log(LOG_ERR, "error creating PUB socket");
    exit(cleanup(&nmea2000_sub, EXIT_FAILURE));
  }

  loop = sbp_get_loop();
  if (loop == nullptr) {
    exit(cleanup(&nmea2000_sub, EXIT_FAILURE));
  }

  nmea2000_sub = pk_endpoint_create(NMEA2000_SUB_ENDPOINT, PK_ENDPOINT_SUB);
  if (nmea2000_sub == nullptr) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(cleanup(&nmea2000_sub, EXIT_FAILURE));
  }

  if (pk_loop_endpoint_reader_add(loop, nmea2000_sub, nmea2000_reader_handler, settings_ctx) == nullptr) {
    piksi_log(LOG_ERR, "error adding reader");
    exit(cleanup(&nmea2000_sub, EXIT_FAILURE));
  }

  // // Put the CAN sockets into ZMQ pollitems.
  // zmq_pollitem_t pollitem_can0 = {};
  // zmq_pollitem_t pollitem_can1 = {};
  // if(debug) {
  //   pollitem_can0.events = ZMQ_POLLIN;
  //   pollitem_can1.events = ZMQ_POLLIN;
  //   pollitem_can0.fd = socket_can0;
  //   pollitem_can1.fd = socket_can1;

  //   // Add CAN pollers to the zloop.
  //   zloop_poller(sbp_zmq_pubsub_zloop_get(ctx), &pollitem_can0,
  //                callback_can_debug, const_cast<char *>(cInterfaceNameCan0));
  //   zloop_poller(sbp_zmq_pubsub_zloop_get(ctx), &pollitem_can1,
  //                callback_can_debug, const_cast<char *>(cInterfaceNameCan1));
  // }

  // Register callbacks for SBP messages.
  // piksi_check(sbp_callback_register(SBP_MSG_TRACKING_STATE,
  //                                   callback_sbp_tracking_state,
  //                                   sbp_zmq_pubsub_zloop_get(ctx)),
  //             "Could not register callback. Message: %" PRIu16,
  //             SBP_MSG_TRACKING_STATE);
  // piksi_check(sbp_callback_register(SBP_MSG_UTC_TIME, callback_sbp_utc_time,
  //                                   sbp_zmq_pubsub_zloop_get(ctx)),
  //             "Could not register callback. Message: %" PRIu16,
  //             SBP_MSG_UTC_TIME);
  // piksi_check(sbp_callback_register(SBP_MSG_BASELINE_HEADING,
  //                                   callback_sbp_baseline_heading,
  //                                   sbp_zmq_pubsub_zloop_get(ctx)),
  //             "Could not register callback. Message: %" PRIu16,
  //             SBP_MSG_BASELINE_HEADING);
  // piksi_check(sbp_callback_register(SBP_MSG_POS_LLH, callback_sbp_pos_llh,
  //                                   sbp_zmq_pubsub_zloop_get(ctx)),
  //             "Could not register callback. Message: %" PRIu16,
  //             SBP_MSG_POS_LLH);
  // piksi_check(sbp_callback_register(SBP_MSG_VEL_NED, callback_sbp_vel_ned,
  //                                   sbp_zmq_pubsub_zloop_get(ctx)),
  //             "Could not register callback. Message: %" PRIu16,
  //             SBP_MSG_VEL_NED);
  // piksi_check(sbp_callback_register(SBP_MSG_DOPS, callback_sbp_dops,
  //                                   sbp_zmq_pubsub_zloop_get(ctx)),
  //             "Could not register callback. Message: %" PRIu16, SBP_MSG_DOPS);
  // piksi_check(sbp_callback_register(SBP_MSG_HEARTBEAT, callback_sbp_heartbeat,
  //                                   sbp_zmq_pubsub_zloop_get(ctx)),
  //             "Could not register callback. Message: %" PRIu16,
  //             SBP_MSG_HEARTBEAT);

  // Register callbacks for settings.
  settings_ctx = sbp_get_settings_ctx();

  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "enable", &setting_n2k_enable,
  //                               sizeof(setting_n2k_enable), SETTINGS_TYPE_BOOL,
  //                     /*callback=*/nullptr, /*arg=*/nullptr),
  //             "Could not register setting_sbp_tracking setting.");
  // auto callback_model_it_setting = [](void *arg) {
  //     UNUSED(arg);
  //     // Update all the info in this callback.
  //     NMEA2000.SetProductInformation(manufacturers_model_serial_code,
  //                                    cNmeaManufacturersProductCode,
  //                                    manufacturers_model_id,
  //                                    manufacturers_software_version_code,
  //                                    cManufacturersModelVersion, cLoadEquivalency,
  //                                    cNmeaNetworkMessageDatabaseVersion,
  //                                    cNMEA2000CertificationLevel);
  //     return 0;
  // };
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "Manufacturer's Model ID",
  //                               manufacturers_model_id,
  //                               sizeof(manufacturers_model_id),
  //                               SETTINGS_TYPE_STRING, callback_model_it_setting,
  //                               nullptr),
  //             "Could not register manufacturers_model_id setting.");

  // // These are settings that control which SBP callbacks are processed.
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_sbp_tracking", &setting_sbp_tracking,
  //                               sizeof(setting_sbp_tracking),
  //                               SETTINGS_TYPE_BOOL, /*callback=*/nullptr,
  //                     /*arg=*/nullptr),
  //             "Could not register setting_sbp_tracking setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_sbp_utc", &setting_sbp_utc,
  //                               sizeof(setting_sbp_utc), SETTINGS_TYPE_BOOL,
  //                     /*callback=*/nullptr, nullptr),
  //             "Could not register setting_sbp_utc setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_sbp_heading", &setting_sbp_heading,
  //                               sizeof(setting_sbp_heading), SETTINGS_TYPE_BOOL,
  //                     /*callback=*/nullptr, /*arg=*/nullptr),
  //             "Could not register setting_sbp_heading setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_sbp_llh", &setting_sbp_llh,
  //                               sizeof(setting_sbp_llh), SETTINGS_TYPE_BOOL,
  //                     /*callback=*/nullptr, /*arg=*/nullptr),
  //             "Could not register setting_sbp_llh setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_sbp_vel_ned", &setting_sbp_vel_ned,
  //                               sizeof(setting_sbp_vel_ned), SETTINGS_TYPE_BOOL,
  //                     /*callback=*/nullptr, /*arg=*/nullptr),
  //             "Could not register setting_sbp_vel_ned setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_sbp_dops", &setting_sbp_dops,
  //                               sizeof(setting_sbp_dops), SETTINGS_TYPE_BOOL,
  //                     /*callback=*/nullptr, /*arg=*/nullptr),
  //             "Could not register setting_sbp_dops setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_sbp_heartbeat", &setting_sbp_heartbeat,
  //                               sizeof(setting_sbp_heartbeat),
  //                               SETTINGS_TYPE_BOOL, /*callback=*/nullptr,
  //                     /*arg=*/nullptr),
  //             "Could not register setting_sbp_heartbeat setting.");

  // // These are settings that control which NMEA2000 messages are sent.
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_n2k_126992", &setting_n2k_126992,
  //                               sizeof(setting_n2k_126992),
  //                               SETTINGS_TYPE_BOOL, /*callback=*/nullptr,
  //                     /*arg=*/nullptr),
  //             "Could not register setting_n2k_126992 setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_n2k_127250", &setting_n2k_127250,
  //                               sizeof(setting_n2k_127250),
  //                               SETTINGS_TYPE_BOOL, /*callback=*/nullptr,
  //                     /*arg=*/nullptr),
  //             "Could not register setting_n2k_127250 setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_n2k_129025", &setting_n2k_129025,
  //                               sizeof(setting_n2k_129025),
  //                               SETTINGS_TYPE_BOOL, /*callback=*/nullptr,
  //                     /*arg=*/nullptr),
  //             "Could not register setting_n2k_129025 setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_n2k_129026", &setting_n2k_129026,
  //                               sizeof(setting_n2k_129026),
  //                               SETTINGS_TYPE_BOOL, /*callback=*/nullptr,
  //                     /*arg=*/nullptr),
  //             "Could not register setting_n2k_129026 setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_n2k_129029", &setting_n2k_129029,
  //                               sizeof(setting_n2k_129029),
  //                               SETTINGS_TYPE_BOOL, /*callback=*/nullptr,
  //                     /*arg=*/nullptr),
  //             "Could not register setting_n2k_129029 setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_n2k_129539", &setting_n2k_129539,
  //                               sizeof(setting_n2k_129539),
  //                               SETTINGS_TYPE_BOOL, /*callback=*/nullptr,
  //                     /*arg=*/nullptr),
  //             "Could not register setting_n2k_129539 setting.");
  // piksi_check(settings_register(ctx_settings, cSettingsCategoryName,
  //                               "setting_n2k_129540", &setting_n2k_129540,
  //                               sizeof(setting_n2k_129540),
  //                               SETTINGS_TYPE_BOOL, /*callback=*/nullptr,
  //                     /*arg=*/nullptr),
  //             "Could not register setting_n2k_129540 setting.");

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

  // while (zmq_simple_loop_timeout(sbp_zmq_pubsub_zloop_get(ctx), 50) != -1) {
  //   NMEA2000.ParseMessages();
  // }

  sbp_run();

  exit(cleanup(&nmea2000_sub, EXIT_SUCCESS));
}

static int cleanup(pk_endpoint_t **nmea2000_ept_loc, int status)
{
  pk_endpoint_destroy(nmea2000_ept_loc);
  sbp_deinit();
  logging_deinit();
  return status;
}
