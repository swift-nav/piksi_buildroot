/*
 * Copyright (C) 2016-2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/settings.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/runit.h>
#include <libpiksi/networking.h>
#include <libsbp/piksi.h>
#include <libsbp/logging.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>

#define PROGRAM_NAME "piksi_system_daemon"

#define PUB_ENDPOINT ">tcp://127.0.0.1:43011"
#define SUB_ENDPOINT ">tcp://127.0.0.1:43010"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255
#define SBP_MAX_NETWORK_INTERFACES 10

#define RUNIT_SERVICE_DIR "/var/run/piksi_system_daemon/sv"

//#define DEBUG_PIKSI_SYSTEM_DAEMON

static const char const * ip_mode_enum_names[] = {"Static", "DHCP", NULL};
enum {IP_CFG_STATIC, IP_CFG_DHCP};
static u8 eth_ip_mode = IP_CFG_STATIC;
static char eth_ip_addr[16] = "192.168.0.222";
static char eth_netmask[16] = "255.255.255.0";
static char eth_gateway[16] = "192.168.0.1";

static void eth_update_config(void)
{
  if (eth_ip_mode == IP_CFG_DHCP) {
    system("sudo /etc/init.d/update_eth0_config dhcp");
  } else {
    char command[1024] = {0};
    size_t count = snprintf(command, sizeof(command),
                            "sudo /etc/init.d/update_eth0_config static %s %s %s",
                            eth_ip_addr, eth_netmask, eth_gateway);
    assert( count < sizeof(command) );
    system(command);
  }
}

static int eth_ip_mode_notify(void *context)
{
  (void)context;
  eth_update_config();
  return 0;
}

static int eth_ip_config_notify(void *context)
{
  char *ip = (char *)context;

  if (inet_addr(ip) == INADDR_NONE) {
    return -1;
  }

  eth_update_config();
  return 0;
}

static void sbp_network_req(u16 sender_id, u8 len, u8 msg_[], void* context)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = (sbp_zmq_pubsub_ctx_t *)context;

  msg_network_state_resp_t interfaces[SBP_MAX_NETWORK_INTERFACES];
  memset(interfaces, 0, sizeof(interfaces));

  u8 total_interfaces = 0;
  query_network_state(interfaces, SBP_MAX_NETWORK_INTERFACES, &total_interfaces);

  for (int i = 0; i < total_interfaces; i++)
  {
    sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(pubsub_ctx),
                    SBP_MSG_NETWORK_STATE_RESP, sizeof(msg_network_state_resp_t),
                    (u8*)&interfaces[i]);
  }
}

static void sbp_command(u16 sender_id, u8 len, u8 msg_[], void* context)
{
  (void) sender_id;

  sbp_zmq_pubsub_ctx_t *pubsub_ctx = (sbp_zmq_pubsub_ctx_t *)context;

  /* TODO As more commands are added in the future the command field will need
   * to be parsed into a command and arguments, and restrictions imposed
   * on what commands and arguments are legal.  For now we only accept two 
   * canned command.
   */

  msg_[len] = 0;
  msg_command_req_t *msg = (msg_command_req_t *)msg_;

  if (strcmp(msg->command, "ntrip_daemon --reconnect") == 0) {
    system("ntrip_daemon --reconnect");
    return;
  }

  if (strcmp(msg->command, "upgrade_tool upgrade.image_set.bin") != 0) {
    msg_command_resp_t resp = {
      .sequence = msg->sequence,
      .code = (u32)-1,
    };
    sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(pubsub_ctx),
                    SBP_MSG_COMMAND_RESP, sizeof(resp), (u8*)&resp);
    return;
  }

  const char* upgrade_cmd =
    "sh -c 'set -o pipefail;                                      "
    "       sudo upgrade_tool --debug /data/upgrade.image_set.bin "
    "         | sbp_log --info'                                   ";

  char finish_cmd[1024];
  size_t count = snprintf(finish_cmd, sizeof(finish_cmd),
                          "sbp_cmd_resp --sequence %u --status $1",
                          msg->sequence);
  assert( count < sizeof(finish_cmd) );

#ifdef DEBUG_PIKSI_SYSTEM_DAEMON
  piksi_log(LOG_DEBUG, "%s: update_tool command sequence: %u, command string: %s",
            __FUNCTION__, msg->sequence, finish_cmd);
#endif

  runit_config_t cfg = (runit_config_t) {
    .service_dir    = RUNIT_SERVICE_DIR,
    .service_name   = "upgrade_tool",
    .command_line   = upgrade_cmd,
    .finish_command = finish_cmd,
    .restart        = false,
  };

  start_runit_service(&cfg);
}

static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    piksi_log(LOG_ERR, "error opening %s", filename);
    return -1;
  }

  bool success = (fgets(str, str_size, fp) != NULL);

  fclose(fp);

  if (!success) {
    piksi_log(LOG_ERR, "error reading %s", filename);
    return -1;
  }

  return 0;
}

static int date_string_get(const char *timestamp_string,
                           char *date_string, size_t date_string_size)
{
  time_t timestamp = strtoul(timestamp_string, NULL, 10);
  if (strftime(date_string, date_string_size,
               "%Y-%m-%d %H:%M:%S %Z", localtime(&timestamp)) == 0) {
    piksi_log(LOG_ERR, "error parsing timestamp");
    return -1;
  }

  return 0;
}

static void img_tbl_settings_setup(settings_ctx_t *settings_ctx)
{
  char name_string[64];
  if (file_read_string("/img_tbl/boot/name",
                       name_string, sizeof(name_string)) == 0) {
    /* firmware_build_id contains the full image name */
    static char firmware_build_id[sizeof(name_string)];
    strncpy(firmware_build_id, name_string, sizeof(firmware_build_id));
    settings_register_readonly(settings_ctx, "system_info", "firmware_build_id",
                               firmware_build_id, sizeof(firmware_build_id),
                               SETTINGS_TYPE_STRING);

    /* firmware_version contains everything before the git hash */
    static char firmware_version[sizeof(name_string)];
    strncpy(firmware_version, name_string, sizeof(firmware_version));
    char *sep = strstr(firmware_version, "-g");
    if (sep != NULL) {
      *sep = 0;
    }
    settings_register_readonly(settings_ctx, "system_info", "firmware_version",
                               firmware_version, sizeof(firmware_version),
                               SETTINGS_TYPE_STRING);
  }

  char timestamp_string[32];
  if (file_read_string("/img_tbl/boot/timestamp", timestamp_string,
                       sizeof(timestamp_string)) == 0) {
    static char firmware_build_date[128];
    if (date_string_get(timestamp_string, firmware_build_date,
                        sizeof(firmware_build_date)) == 0) {
      settings_register_readonly(settings_ctx, "system_info", "firmware_build_date",
                                 firmware_build_date, sizeof(firmware_build_date),
                                 SETTINGS_TYPE_STRING);
    }
  }

  char loader_name_string[64];
  if (file_read_string("/img_tbl/loader/name",
                       loader_name_string, sizeof(loader_name_string)) == 0) {
    static char loader_build_id[sizeof(loader_name_string)];
    strncpy(loader_build_id, loader_name_string, sizeof(loader_build_id));
    settings_register_readonly(settings_ctx, "system_info", "loader_build_id",
                               loader_build_id, sizeof(loader_build_id),
                               SETTINGS_TYPE_STRING);
  }

  char loader_timestamp_string[32];
  if (file_read_string("/img_tbl/loader/timestamp", loader_timestamp_string,
                       sizeof(loader_timestamp_string)) == 0) {
    static char loader_build_date[128];
    if (date_string_get(loader_timestamp_string, loader_build_date,
                        sizeof(loader_build_date)) == 0) {
      settings_register_readonly(settings_ctx, "system_info", "loader_build_date",
                                 loader_build_date, sizeof(loader_build_date),
                                 SETTINGS_TYPE_STRING);
    }
  }
}

static void reset_callback(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id; (void)context;

  const char* reset_settings = "";

  /* Reset settings to defaults if requested */
  if (len == sizeof(msg_reset_t)) {
    const msg_reset_t *msg = (const void*)msg_;
    if (msg->flags & 1) {
      reset_settings = "clear_settings";
    }
  }

  char command[1024] = {0};
  size_t count = snprintf(command, sizeof(command),
                          "sudo /etc/init.d/do_sbp_msg_reset %s",
                          reset_settings);
  assert( count < sizeof(command) );

  system(command);
}

static const char * const system_time_sources[] = {
  "GPS+NTP", "GPS", "NTP", NULL
};
enum {SYSTEM_TIME_GPS_NTP, SYSTEM_TIME_GPS, SYSTEM_TIME_NTP};
static u8 system_time_src;

static int system_time_src_notify(void *context)
{
  static u8 oldsrc;
  const char *ntpconf = NULL;

  if (oldsrc == system_time_src) {
    /* Avoid restarting ntpd if config hasn't changed */
    return 0;
  }

  switch (system_time_src) {
  case SYSTEM_TIME_GPS_NTP:
    ntpconf = "/etc/ntp.conf.gpsntp";
    break;
  case SYSTEM_TIME_GPS:
    ntpconf = "/etc/ntp.conf.gps";
    break;
  case SYSTEM_TIME_NTP:
    ntpconf = "/etc/ntp.conf.ntp";
    break;
  default:
    return -1;
  }

  char command[1024] = {0};
  size_t count = snprintf(command, sizeof(command),
                          "sudo /etc/init.d/update_ntp_config %s",
                          ntpconf);
  assert( count < sizeof(command) );

  return system(command);
}

int main(void)
{
  logging_init(PROGRAM_NAME);

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  /* Set up SBP ZMQ */
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = sbp_zmq_pubsub_create(PUB_ENDPOINT,
                                                           SUB_ENDPOINT);
  if (pubsub_ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  /* Set up settings */
  settings_ctx_t *settings_ctx = settings_create();
  if (settings_ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  if (settings_reader_add(settings_ctx,
                          sbp_zmq_pubsub_zloop_get(pubsub_ctx)) != 0) {
    exit(EXIT_FAILURE);
  }

  settings_type_t settings_type_ip_mode;
  settings_type_register_enum(settings_ctx, ip_mode_enum_names,
                              &settings_type_ip_mode);
  settings_register(settings_ctx, "ethernet", "ip_config_mode", &eth_ip_mode,
                    sizeof(eth_ip_mode), settings_type_ip_mode,
                    eth_ip_mode_notify, &eth_ip_mode);
  settings_register(settings_ctx, "ethernet", "ip_address", &eth_ip_addr,
                    sizeof(eth_ip_addr), SETTINGS_TYPE_STRING,
                    eth_ip_config_notify, &eth_ip_addr);
  settings_register(settings_ctx, "ethernet", "netmask", &eth_netmask,
                    sizeof(eth_netmask), SETTINGS_TYPE_STRING,
                    eth_ip_config_notify, &eth_netmask);
  settings_register(settings_ctx, "ethernet", "gateway", &eth_gateway,
                    sizeof(eth_gateway), SETTINGS_TYPE_STRING,
                    eth_ip_config_notify, &eth_gateway);

  settings_type_t settings_type_time_source;
  settings_type_register_enum(settings_ctx, system_time_sources,
                              &settings_type_time_source);
  settings_register(settings_ctx, "system", "system_time", &system_time_src,
                    sizeof(system_time_src), settings_type_time_source,
                    system_time_src_notify, &system_time_src);

  sbp_zmq_rx_callback_register(sbp_zmq_pubsub_rx_ctx_get(pubsub_ctx),
                               SBP_MSG_RESET, reset_callback, NULL, NULL);
  sbp_zmq_rx_callback_register(sbp_zmq_pubsub_rx_ctx_get(pubsub_ctx),
                               SBP_MSG_RESET_DEP, reset_callback, NULL, NULL);

  img_tbl_settings_setup(settings_ctx);
  sbp_zmq_rx_callback_register(sbp_zmq_pubsub_rx_ctx_get(pubsub_ctx),
                               SBP_MSG_COMMAND_REQ, sbp_command, pubsub_ctx, NULL);
  sbp_zmq_rx_callback_register(sbp_zmq_pubsub_rx_ctx_get(pubsub_ctx),
                               SBP_MSG_NETWORK_STATE_REQ, sbp_network_req,
                               pubsub_ctx, NULL);

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(pubsub_ctx));

  sbp_zmq_pubsub_destroy(&pubsub_ctx);
  settings_destroy(&settings_ctx);
  exit(EXIT_SUCCESS);
}
