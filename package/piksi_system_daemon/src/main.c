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

#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/settings.h>
#include <libpiksi/loop.h>
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
#include <time.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>

#define PROGRAM_NAME "piksi_system_daemon"

#define PUB_ENDPOINT "ipc:///var/run/sockets/firmware.sub"
#define SUB_ENDPOINT "ipc:///var/run/sockets/firmware.pub"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255
#define SBP_MAX_NETWORK_INTERFACES 10

#define STR_BUFFER_SIZE 64
#define DATE_STR_BUFFER_SIZE 128
#define DEFAULT_HW_VERSION "0.0"

static double network_polling_frequency = 0.1;
static double network_polling_retry_frequency = 1;
static bool log_ping_activity = false;

#define RUNIT_SERVICE_DIR "/var/run/piksi_system_daemon/sv"
//#define DEBUG_PIKSI_SYSTEM_DAEMON

#define NETWORK_POLLING_PERIOD_FILE "/var/run/piksi_sys/network_polling_period"
#define NETWORK_POLLING_RETRY_PERIOD_FILE "/var/run/piksi_sys/network_polling_retry_period"
#define ENABLE_PING_LOGGING_FILE "/var/run/piksi_sys/enable_ping_logging"

static const char const *ip_mode_enum_names[] = {"Static", "DHCP", NULL};
enum { IP_CFG_STATIC, IP_CFG_DHCP };
static u8 eth_ip_mode = IP_CFG_STATIC;
static char eth_ip_addr[16] = "192.168.0.222";
static char eth_netmask[16] = "255.255.255.0";
static char eth_gateway[16] = "192.168.0.1";

static void eth_update_config(void)
{
  if (eth_ip_mode == IP_CFG_DHCP) {
    int rc = system("sudo /etc/init.d/update_eth0_config dhcp");
    if (rc != 0) piksi_log(LOG_WARNING, "exit status from update_eth0_config script: %d", rc);
  } else {
    char command[1024] = {0};
    size_t count = snprintf(command,
                            sizeof(command),
                            "sudo /etc/init.d/update_eth0_config static %s %s %s",
                            eth_ip_addr,
                            eth_netmask,
                            eth_gateway);
    assert(count < sizeof(command));
    int rc = system(command);
    if (rc != 0) piksi_log(LOG_WARNING, "exit status from update_eth0_config script: %d", rc);
  }
}

static int eth_ip_mode_notify(void *context)
{
  (void)context;
  eth_update_config();
  return SBP_SETTINGS_WRITE_STATUS_OK;
}

static int eth_ip_config_notify(void *context)
{
  char *ip = (char *)context;

  if (inet_addr(ip) == INADDR_NONE) {
    return SBP_SETTINGS_WRITE_STATUS_VALUE_REJECTED;
  }

  eth_update_config();
  return SBP_SETTINGS_WRITE_STATUS_OK;
}

static void sbp_network_req(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  sbp_pubsub_ctx_t *pubsub_ctx = (sbp_pubsub_ctx_t *)context;

  msg_network_state_resp_t interfaces[SBP_MAX_NETWORK_INTERFACES];
  memset(interfaces, 0, sizeof(interfaces));

  u8 total_interfaces = 0;
  query_network_state(interfaces, SBP_MAX_NETWORK_INTERFACES, &total_interfaces);

  for (int i = 0; i < total_interfaces; i++) {
    sbp_tx_send(sbp_pubsub_tx_ctx_get(pubsub_ctx),
                SBP_MSG_NETWORK_STATE_RESP,
                sizeof(msg_network_state_resp_t),
                (u8 *)&interfaces[i]);
  }
}

static void sbp_command(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;

  sbp_pubsub_ctx_t *pubsub_ctx = (sbp_pubsub_ctx_t *)context;

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

  if (strcmp(msg->command, "upgrade_tool upgrade.image_set.bin") != 0
      && strcmp(msg->command, "spawn_nc") != 0 && strcmp(msg->command, "stream_logs") != 0
      && strcmp(msg->command, "dump_syslog") != 0) {
    msg_command_resp_t resp = {
      .sequence = msg->sequence,
      .code = (u32)-1,
    };
    sbp_tx_send(sbp_pubsub_tx_ctx_get(pubsub_ctx), SBP_MSG_COMMAND_RESP, sizeof(resp), (u8 *)&resp);
    return;
  }

  if (strcmp(msg->command, "spawn_nc") == 0) {

    char spawn_nc[1024];

    size_t count = snprintf(spawn_nc,
                            sizeof(spawn_nc),
                            "sudo -u fileio /usr/bin/spawn_nc; "
                            "sbp_cmd_resp --sequence %u --status $?",
                            msg->sequence);
    assert(count < sizeof(spawn_nc));
    (void)system(spawn_nc);

    return;
  };

  if (strcmp(msg->command, "stream_logs") == 0) {

    char stream_logs[1024];

    size_t count = snprintf(stream_logs,
                            sizeof(stream_logs),
                            "stream_logs; sbp_cmd_resp --sequence %u --status $?",
                            msg->sequence);

    assert(count < sizeof(stream_logs));
    (void)system(stream_logs);

    return;
  };

  if (strcmp(msg->command, "dump_syslog") == 0) {

    char dump_syslog[1024];

    size_t count = snprintf(dump_syslog,
                            sizeof(dump_syslog),
                            "dump_syslog; sbp_cmd_resp --sequence %u --status $?",
                            msg->sequence);

    assert(count < sizeof(dump_syslog));
    (void)system(dump_syslog);

    return;
  }

  const char *upgrade_cmd =
    "sh -c 'set -o pipefail;                                      "
    "       sudo upgrade_tool --debug /data/upgrade.image_set.bin "
    "         | sbp_log --info'                                   ";

  // TODO/DAEMONUSERS: Determine if we need to do anything to this code
  //   in order to incorporate the fix from here:
  //
  //       https://github.com/swift-nav/piksi_buildroot/commit/52b99371e7a43d62da3baa3b82a3b8b56b787ace

  char finish_cmd[1024];
  size_t count =
    snprintf(finish_cmd,
             sizeof(finish_cmd),
             "sh -c \"for i in 1 2; do sbp_cmd_resp --sequence %u --status $1; sleep 1; done;\"",
             msg->sequence);
  assert(count < sizeof(finish_cmd));

  // clang-format off
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
  // clang-format on

  start_runit_service(&cfg);
}

static int date_string_get(const char *timestamp_string, char *date_string, size_t date_string_size)
{
  time_t timestamp = strtoul(timestamp_string, NULL, 10);
  if (strftime(date_string, date_string_size, "%Y-%m-%d %H:%M:%S %Z", localtime(&timestamp)) == 0) {
    piksi_log(LOG_ERR, "error parsing timestamp");
    return -1;
  }

  return 0;
}

static void img_tbl_settings_setup(settings_ctx_t *settings_ctx)
{
  char name_string[STR_BUFFER_SIZE];
  char uimage_string[STR_BUFFER_SIZE];
  bool is_dev = false;
  if (file_read_string("/img_tbl/boot/name", name_string, STR_BUFFER_SIZE) == 0) {

    if (file_read_string("/uimage_ver/name", uimage_string, sizeof(uimage_string)) != 0) {
      sprintf(uimage_string, "unknown_uimage_version");
    }

    static char imageset_build_id[STR_BUFFER_SIZE];
    strncpy(imageset_build_id, name_string, STR_BUFFER_SIZE);
    settings_register_readonly(settings_ctx,
                               "system_info",
                               "imageset_build_id",
                               imageset_build_id,
                               sizeof(imageset_build_id),
                               SETTINGS_TYPE_STRING);

    /* If image_table version starts with DEV, we booted a DEV build
     * If we have DEV:
     *   - firmware_build_id, firwmare_version, firmware_build_date come from uimage version
     *   - "imageset_build_id" always says what imageset was used
     * If we have PROD
         - imageset_build_id and firmware_build_id should be identical
         - we use the firmware_build_id and firmware_build_date from the image_set as they are
           more relevant for production builds and matches legacy behavior
     */

    static char firmware_build_id[STR_BUFFER_SIZE];
    if (strncmp(name_string, "DEV", 3) == 0) {
      is_dev = true;
      strncpy(firmware_build_id, uimage_string, STR_BUFFER_SIZE);
    } else {
      strncpy(firmware_build_id, name_string, STR_BUFFER_SIZE);
      if (strncmp(imageset_build_id, uimage_string, STR_BUFFER_SIZE) != 0) {
        /* we should never get here, in PROD, imageset_version and uimage version should match every
         * time*/
        piksi_log(LOG_ERR, "Production build detected where uimage_build_id != imageset_build_id");
      }
    }

    settings_register_readonly(settings_ctx,
                               "system_info",
                               "firmware_build_id",
                               firmware_build_id,
                               sizeof(firmware_build_id),
                               SETTINGS_TYPE_STRING);

    /* firmware_version contains everything before the git hash */
    static char firmware_version[STR_BUFFER_SIZE];
    strncpy(firmware_version, firmware_build_id, STR_BUFFER_SIZE);
    char *sep = strstr(firmware_version, "-g");
    if (sep != NULL) {
      *sep = 0;
    }
    settings_register_readonly(settings_ctx,
                               "system_info",
                               "firmware_version",
                               firmware_version,
                               sizeof(firmware_version),
                               SETTINGS_TYPE_STRING);
  }
  char timestamp_string[STR_BUFFER_SIZE];
  if (file_read_string(is_dev ? "/uimage_ver/timestamp" : "/img_tbl/boot/timestamp",
                       timestamp_string,
                       STR_BUFFER_SIZE)
      == 0) {
    static char firmware_build_date[DATE_STR_BUFFER_SIZE];
    if (date_string_get(timestamp_string, firmware_build_date, DATE_STR_BUFFER_SIZE) == 0) {
      settings_register_readonly(settings_ctx,
                                 "system_info",
                                 "firmware_build_date",
                                 firmware_build_date,
                                 sizeof(firmware_build_date),
                                 SETTINGS_TYPE_STRING);
    }
  }

  char loader_name_string[STR_BUFFER_SIZE];
  if (file_read_string("/img_tbl/loader/name", loader_name_string, STR_BUFFER_SIZE) == 0) {
    static char loader_build_id[STR_BUFFER_SIZE];
    strncpy(loader_build_id, loader_name_string, STR_BUFFER_SIZE);
    settings_register_readonly(settings_ctx,
                               "system_info",
                               "loader_build_id",
                               loader_build_id,
                               sizeof(loader_build_id),
                               SETTINGS_TYPE_STRING);
  }

  char loader_timestamp_string[STR_BUFFER_SIZE];
  if (file_read_string("/img_tbl/loader/timestamp", loader_timestamp_string, STR_BUFFER_SIZE)
      == 0) {
    static char loader_build_date[DATE_STR_BUFFER_SIZE];
    if (date_string_get(loader_timestamp_string, loader_build_date, DATE_STR_BUFFER_SIZE) == 0) {
      settings_register_readonly(settings_ctx,
                                 "system_info",
                                 "loader_build_date",
                                 loader_build_date,
                                 sizeof(loader_build_date),
                                 SETTINGS_TYPE_STRING);
    }
  }
}

static void hardware_info_settings_setup(settings_ctx_t *settings_ctx)
{
  static char info_string[STR_BUFFER_SIZE] = {0};
  size_t info_string_size = sizeof(info_string);

  if (hw_version_string_get(info_string, info_string_size) != 0) {
    const char *default_str = DEFAULT_HW_VERSION;
    piksi_log(LOG_WARNING,
              "Failed to get hw_version for system_info registration, setting to default: %s",
              default_str);
    if (strlen(default_str) < info_string_size) {
      strncpy(info_string, default_str, info_string_size);
    } else {
      piksi_log(LOG_ERR,
                "Default version string too large for buffer (size of intended string: %d)",
                strlen(default_str));
    }
  }
  if (settings_register_readonly(settings_ctx,
                                 "system_info",
                                 "hw_version",
                                 info_string,
                                 strlen(info_string) + 1,
                                 SETTINGS_TYPE_STRING)
      != 0) {
    piksi_log(LOG_WARNING, "Failed to register hw_version in system_info");
  }

  if (hw_revision_string_get(info_string, info_string_size) != 0) {
    piksi_log(LOG_WARNING, "Failed to get hw_revision for system_info registration");
  } else if (settings_register_readonly(settings_ctx,
                                        "system_info",
                                        "hw_revision",
                                        info_string,
                                        strlen(info_string) + 1,
                                        SETTINGS_TYPE_STRING)
             != 0) {
    piksi_log(LOG_WARNING, "Failed to register hw_revision in system_info");
  }

  if (hw_variant_string_get(info_string, info_string_size) != 0) {
    piksi_log(LOG_WARNING, "Failed to get hw_variant for system_info registration");
  } else if (settings_register_readonly(settings_ctx,
                                        "system_info",
                                        "hw_variant",
                                        info_string,
                                        strlen(info_string) + 1,
                                        SETTINGS_TYPE_STRING)
             != 0) {
    piksi_log(LOG_WARNING, "Failed to register hw_variant in system_info");
  }

  if (product_id_string_get(info_string, info_string_size) != 0) {
    piksi_log(LOG_WARNING, "Failed to get product_id for system_info registration");
  } else if (settings_register_readonly(settings_ctx,
                                        "system_info",
                                        "product_id",
                                        info_string,
                                        strlen(info_string) + 1,
                                        SETTINGS_TYPE_STRING)
             != 0) {
    piksi_log(LOG_WARNING, "Failed to register product_id in system_info");
  }
}

static void reset_callback(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  (void)context;

  const char *reset_settings = "";

  /* Reset settings to defaults if requested */
  if (len == sizeof(msg_reset_t)) {
    const msg_reset_t *msg = (const void *)msg_;
    if (msg->flags & 1) {
      reset_settings = "clear_settings";
    }
  }

  char command[1024] = {0};
  size_t count =
    snprintf(command, sizeof(command), "sudo /etc/init.d/do_sbp_msg_reset %s", reset_settings);
  assert(count < sizeof(command));

  system(command);
}

static const char *const system_time_sources[] = {"GPS+NTP", "GPS", "NTP", NULL};
enum { SYSTEM_TIME_GPS_NTP, SYSTEM_TIME_GPS, SYSTEM_TIME_NTP };
static u8 system_time_src;

static int system_time_src_notify(void *context)
{
  static u8 oldsrc;
  const char *ntpconf = NULL;

  if (oldsrc == system_time_src) {
    /* Avoid restarting ntpd if config hasn't changed */
    return SBP_SETTINGS_WRITE_STATUS_OK;
  }

  switch (system_time_src) {
  case SYSTEM_TIME_GPS_NTP: ntpconf = "/etc/ntp.conf.gpsntp"; break;
  case SYSTEM_TIME_GPS: ntpconf = "/etc/ntp.conf.gps"; break;
  case SYSTEM_TIME_NTP: ntpconf = "/etc/ntp.conf.ntp"; break;
  default: return SBP_SETTINGS_WRITE_STATUS_VALUE_REJECTED;
  }

  char command[1024] = {0};
  size_t count =
    snprintf(command, sizeof(command), "sudo /etc/init.d/update_ntp_config %s", ntpconf);
  assert(count < sizeof(command));

  if (system(command) != 0) {
    return SBP_SETTINGS_WRITE_STATUS_SERVICE_FAILED;
  }
  return SBP_SETTINGS_WRITE_STATUS_OK;
}

static int network_polling_notify(void *context)
{
  (void)context;

  struct {
    char buf[32];
    int count;
    const char *name;
  } formatters[3] = {

    [0].name = "network_polling_frequency",
    [0].count = ({
      snprintf(formatters[0].buf,
               sizeof(formatters[0].buf),
               "%.02f",
               (1.0 / network_polling_frequency));
    }),

    [1].name = "network_polling_retry_frequency",
    [1].count = ({
      snprintf(formatters[1].buf,
               sizeof(formatters[1].buf),
               "%.02f",
               (1.0 / network_polling_retry_frequency));
    }),

    [2].name = "log_ping_activity",
    [2].count = ({
      snprintf(formatters[2].buf, sizeof(formatters[2].buf), "%s", log_ping_activity ? "y" : "");
    }),
  };

  for (size_t x = 0; x < COUNT_OF(formatters); x++) {
    if ((size_t)formatters[x].count >= sizeof(formatters[x].buf)) {
      piksi_log(LOG_ERR | LOG_SBP,
                "buffer overflow while formatting setting '%s'",
                formatters[x].name);
      return SBP_SETTINGS_WRITE_STATUS_VALUE_REJECTED;
    }
  }

  // clang-format off
  struct { const char* filename; const char* value; } settings_value_files[3] = {
    [0].filename = NETWORK_POLLING_PERIOD_FILE,       [0].value = formatters[0].buf,
    [1].filename = NETWORK_POLLING_RETRY_PERIOD_FILE, [1].value = formatters[1].buf,
    [2].filename = ENABLE_PING_LOGGING_FILE,          [2].value = formatters[2].buf,
  };
  // clang-format on

  for (size_t x = 0; x < COUNT_OF(settings_value_files); x++) {
    FILE *fp = fopen(settings_value_files[x].filename, "w");
    if (fp == NULL) {
      piksi_log(LOG_ERR | LOG_SBP,
                "failed to open '%s': %s",
                settings_value_files[x].filename,
                strerror(errno));
      continue;
    }
    int count = fprintf(fp, "%s", settings_value_files[x].value);
    if (count < 0) {
      piksi_log(LOG_ERR | LOG_SBP,
                "error writing to '%s': %s",
                settings_value_files[x].filename,
                strerror(errno));
    }
    fclose(fp);
  }

  return SBP_SETTINGS_WRITE_STATUS_OK;
}

int main(void)
{
  logging_init(PROGRAM_NAME);

  pk_loop_t *loop = pk_loop_create();
  if (loop == NULL) {
    exit(EXIT_FAILURE);
  }

  /* Set up SBP */
  sbp_pubsub_ctx_t *pubsub_ctx = sbp_pubsub_create(PUB_ENDPOINT, SUB_ENDPOINT);
  if (pubsub_ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  /* Attach pubsub to loop */
  if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(pubsub_ctx), loop) != 0) {
    exit(EXIT_FAILURE);
  }

  /* Set up settings */
  settings_ctx_t *settings_ctx = settings_create();
  if (settings_ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  if (settings_attach(settings_ctx, loop) != 0) {
    exit(EXIT_FAILURE);
  }

  settings_type_t settings_type_ip_mode;
  settings_type_register_enum(settings_ctx, ip_mode_enum_names, &settings_type_ip_mode);
  settings_register(settings_ctx,
                    "ethernet",
                    "ip_config_mode",
                    &eth_ip_mode,
                    sizeof(eth_ip_mode),
                    settings_type_ip_mode,
                    eth_ip_mode_notify,
                    &eth_ip_mode);
  settings_register(settings_ctx,
                    "ethernet",
                    "ip_address",
                    &eth_ip_addr,
                    sizeof(eth_ip_addr),
                    SETTINGS_TYPE_STRING,
                    eth_ip_config_notify,
                    &eth_ip_addr);
  settings_register(settings_ctx,
                    "ethernet",
                    "netmask",
                    &eth_netmask,
                    sizeof(eth_netmask),
                    SETTINGS_TYPE_STRING,
                    eth_ip_config_notify,
                    &eth_netmask);
  settings_register(settings_ctx,
                    "ethernet",
                    "gateway",
                    &eth_gateway,
                    sizeof(eth_gateway),
                    SETTINGS_TYPE_STRING,
                    eth_ip_config_notify,
                    &eth_gateway);

  settings_type_t settings_type_time_source;
  settings_type_register_enum(settings_ctx, system_time_sources, &settings_type_time_source);
  settings_register(settings_ctx,
                    "system",
                    "system_time",
                    &system_time_src,
                    sizeof(system_time_src),
                    settings_type_time_source,
                    system_time_src_notify,
                    &system_time_src);

  settings_register(settings_ctx,
                    "system",
                    "connectivity_check_frequency",
                    &network_polling_frequency,
                    sizeof(network_polling_frequency),
                    SETTINGS_TYPE_FLOAT,
                    network_polling_notify,
                    NULL);
  settings_register(settings_ctx,
                    "system",
                    "connectivity_retry_frequency",
                    &network_polling_retry_frequency,
                    sizeof(network_polling_retry_frequency),
                    SETTINGS_TYPE_FLOAT,
                    network_polling_notify,
                    NULL);
  settings_register(settings_ctx,
                    "system",
                    "log_ping_activity",
                    &log_ping_activity,
                    sizeof(log_ping_activity),
                    SETTINGS_TYPE_BOOL,
                    network_polling_notify,
                    NULL);

  sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(pubsub_ctx),
                           SBP_MSG_RESET,
                           reset_callback,
                           NULL,
                           NULL);
  sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(pubsub_ctx),
                           SBP_MSG_RESET_DEP,
                           reset_callback,
                           NULL,
                           NULL);

  img_tbl_settings_setup(settings_ctx);
  sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(pubsub_ctx),
                           SBP_MSG_COMMAND_REQ,
                           sbp_command,
                           pubsub_ctx,
                           NULL);
  sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(pubsub_ctx),
                           SBP_MSG_NETWORK_STATE_REQ,
                           sbp_network_req,
                           pubsub_ctx,
                           NULL);
  hardware_info_settings_setup(settings_ctx);

  pk_loop_run_simple(loop);

  pk_loop_destroy(&loop);
  sbp_pubsub_destroy(&pubsub_ctx);
  settings_destroy(&settings_ctx);
  exit(EXIT_SUCCESS);
}
