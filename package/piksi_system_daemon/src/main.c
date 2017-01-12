/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sbp_zmq.h>
#include <sbp_settings.h>
#include <libsbp/piksi.h>
#include <libsbp/logging.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <signal.h>

#include "whitelists.h"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

static sbp_zmq_state_t sbp_zmq_state;

static int settings_msg_send(u16 msg_type, u8 len, u8 *payload)
{
  return sbp_zmq_message_send(&sbp_zmq_state, msg_type, len, payload);
}

static int settings_msg_cb_register(u16 msg_type,
                                    sbp_msg_callback_t cb, void *context,
                                    sbp_msg_callbacks_node_t **node)
{
  return sbp_zmq_callback_register(&sbp_zmq_state, msg_type, cb, context, node);
}

static int settings_msg_cb_remove(sbp_msg_callbacks_node_t *node)
{
  return sbp_zmq_callback_remove(&sbp_zmq_state, node);
}

static int settings_msg_loop_timeout(u32 timeout_ms)
{
  return sbp_zmq_loop_timeout(&sbp_zmq_state, timeout_ms);
}

static int settings_msg_loop_interrupt(void)
{
  return sbp_zmq_loop_interrupt(&sbp_zmq_state);
}

static int uart0_baudrate = 115200;
static int uart1_baudrate = 115200;

static bool baudrate_notify(struct setting *s, const char *val)
{
  int baudrate;
  bool ret = s->type->from_string(s->type->priv, &baudrate, s->len, val);
  if (!ret) {
    return false;
  }

  const char *dev = NULL;
  if (s->addr == &uart0_baudrate) {
    dev = "/dev/ttyPS0";
  } else if (s->addr == &uart1_baudrate) {
    dev = "/dev/ttyPS1";
  } else {
    return false;
  }
  char cmd[80];
  snprintf(cmd, sizeof(cmd), "stty -F %s %d", dev, baudrate);
  ret = system(cmd) == 0;

  if (ret) {
    *(int*)s->addr = baudrate;
  }
  return ret;
}

static const char const * port_mode_enum[] = {"SBP", "NMEA", NULL};
static struct setting_type port_mode_settings_type;
static int TYPE_PORT_MODE = 0;
enum {PORT_MODE_SBP, PORT_MODE_NMEA};
static u8 uart0_mode = PORT_MODE_SBP;
static u8 uart1_mode = PORT_MODE_SBP;
static pid_t uart0_adapter_pid = 0;
static pid_t uart1_adapter_pid = 0;

bool port_mode_notify(struct setting *s, const char *val)
{
  u8 port_mode;
  bool ret = s->type->from_string(s->type->priv, &port_mode, s->len, val);
  if (!ret) {
    return false;
  }

  const char *dev = NULL;
  const char *opts = NULL;
  const char *opts_sbp = NULL;
  pid_t *pid;
  if (s->addr == &uart0_mode) {
    dev = "/dev/ttyPS0";
    opts = "";
    opts_sbp = "-f sbp --filter-out sbp --filter-out-config /etc/uart0_filter_out_config";
    pid = &uart0_adapter_pid;
  } else if (s->addr == &uart1_mode) {
    dev = "/dev/ttyPS1";
    opts = "";
    opts_sbp = "-f sbp --filter-out sbp --filter-out-config /etc/uart1_filter_out_config";
    pid = &uart1_adapter_pid;
  } else {
    return false;
  }

  const char *mode_opts = NULL;
  u16 zmq_port_pub = 0;
  u16 zmq_port_sub = 0;
  switch (port_mode) {
  case PORT_MODE_SBP:
    mode_opts = opts_sbp;
    zmq_port_pub = 43031;
    zmq_port_sub = 43030;
    break;
  case PORT_MODE_NMEA:
    mode_opts = "";
    zmq_port_pub = 44031;
    zmq_port_sub = 44030;
    break;
  default:
    return false;
  }

  /* Kill the old zmq_adapter, if it exists. */
  if (*pid) {
    /* TODO: This is an ugly hack to work around an apparent race condition in
     * the way zmq_adapter handles SIGTERM while starting up. Delay before
     * killing to give zmq_adapter time to get its act together. */
    nanosleep((const struct timespec[]){{0, 200000000L}}, NULL);
    int ret = kill(*pid, SIGTERM);
    printf("Killing zmq_adapter with PID: %d (kill returned %d, errno %d)\n",
           *pid, ret, errno);
  }

  /* Prepare the command used to launch zmq_adapter. */
  char cmd[200];
  snprintf(cmd, sizeof(cmd),
           "zmq_adapter --file %s "
           "-p >tcp://127.0.0.1:%d "
           "-s >tcp://127.0.0.1:%d %s %s",
           dev, zmq_port_pub, zmq_port_sub, opts, mode_opts);

  printf("Starting zmq_adapter: %s\n", cmd);

  /* Split the command on each space for argv */
  char *args[100] = {0};
  args[0] = strtok(cmd, " ");
  for (u8 i=1; (args[i] = strtok(NULL, " ")) && i < 100; i++);

  /* Create a new zmq_adapter. */
  if (!(*pid = fork())) {
    execvp(args[0], args);
  }

  printf("zmq_adapter started with PID: %d\n", *pid);

  if (*pid < 0) {
    /* fork() failed */
    return false;
  }

  return true;
}

static const char const * ip_mode_enum[] = {"Static", "DHCP", NULL};
static struct setting_type ip_mode_settings_type;
static int TYPE_IP_MODE = 0;
enum {IP_CFG_STATIC, IP_CFG_DHCP};
static u8 eth_ip_mode = IP_CFG_STATIC;
static char eth_ip_addr[16] = "192.168.0.222";
static char eth_netmask[16] = "255.255.255.0";
static char eth_gateway[16] = "192.168.0.1";

static void eth_update_config(void)
{
  system("ifdown eth0");

  FILE *interfaces = fopen("/etc/network/interfaces", "w");
  if (eth_ip_mode == IP_CFG_DHCP) {
    fprintf(interfaces, "iface eth0 inet dhcp\n");
  } else {
    fprintf(interfaces, "iface eth0 inet static\n");
    fprintf(interfaces, "\taddress %s\n", eth_ip_addr);
    fprintf(interfaces, "\tnetmask %s\n", eth_netmask);
    fprintf(interfaces, "\tgateway %s\n", eth_gateway);
  }
  fclose(interfaces);

  system("ifup eth0");
}

static bool eth_ip_mode_notify(struct setting *s, const char *val)
{
  bool ret = settings_default_notify(s, val);
  if (ret) {
    eth_update_config();
  }
  return ret;
}

static bool eth_ip_config_notify(struct setting *s, const char *val)
{
  if (inet_addr(val) == INADDR_NONE)
    return false;

  bool ret = settings_default_notify(s, val);
  if (ret) {
    eth_update_config();
  }
  return ret;
}

struct shell_cmd_ctx {
  FILE *pipe;
  u32 sequence;
  zloop_t *loop;
  zmq_pollitem_t pollitem;
};

/* We use this unbuffered fgets function alternative so that the czmq loop
 * will keep calling us with output until we've read it all.
 */
static int raw_fgets(char *str, size_t len, FILE *stream)
{
  int fd = fileno(stream);
  size_t i;
  len--;
  for (i = 0; i < len; i++) {
    int r = read(fd, &str[i], 1);
    if (r < 0)
      return r;
    if ((r == 0) || (str[i] == '\n'))
      break;
  }
  str[i++] = 0;
  return i > 1 ? i : 0;
}

static int command_output(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
  struct shell_cmd_ctx *ctx = arg;
  msg_log_t *msg = alloca(256);
  msg->level = 6;

  if (raw_fgets(msg->text,
                SBP_FRAMING_MAX_PAYLOAD_SIZE - offsetof(msg_log_t, text),
                ctx->pipe) > 0) {
    sbp_zmq_message_send(&sbp_zmq_state, SBP_MSG_LOG,
                         sizeof(*msg) + strlen(msg->text), (void*)msg);
  } else {
    /* Broken pipe, clean up and send exit code */
    msg_command_resp_t resp = {
      .sequence = ctx->sequence,
      .code = pclose(ctx->pipe),
    };
    sbp_zmq_message_send(&sbp_zmq_state, SBP_MSG_COMMAND_RESP,
                         sizeof(resp), (void*)&resp);
    zloop_poller_end(ctx->loop, item);
    free(ctx);
  }
  return 0;
}

static void sbp_command(u16 sender_id, u8 len, u8 msg_[], void* context)
{
  msg_[len] = 0;
  msg_command_req_t *msg = (msg_command_req_t *)msg_;
  /* TODO As more commands are added in the future the command field will need
   * to be parsed into a command and arguments, and restrictions imposed
   * on what commands and arguments are legal.  For now we only accept one
   * canned command.
   */
  if (strcmp(msg->command, "upgrade_tool upgrade.image_set.bin") != 0) {
    msg_command_resp_t resp = {
      .sequence = msg->sequence,
      .code = (u32)-1,
    };
    sbp_zmq_message_send(&sbp_zmq_state, SBP_MSG_COMMAND_RESP,
                         sizeof(resp), (u8*)&resp);
    return;
  }
  struct shell_cmd_ctx *ctx = calloc(1, sizeof(*ctx));
  ctx->pipe = popen(msg->command, "r");
  ctx->sequence = msg->sequence;
  ctx->loop = sbp_zmq_loop_get(&sbp_zmq_state);
  ctx->pollitem.fd = fileno(ctx->pipe);
  ctx->pollitem.events = ZMQ_POLLIN;
  int arg = O_NONBLOCK;
  fcntl(fileno(ctx->pipe), F_SETFL, &arg);
  zloop_poller(ctx->loop, &ctx->pollitem, command_output, ctx);
}

static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("error opening %s\n", filename);
    return -1;
  }

  bool success = (fgets(str, str_size, fp) != NULL);

  fclose(fp);

  if (!success) {
    printf("error reading %s\n", filename);
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
    printf("error parsing timestamp\n");
    return -1;
  }

  return 0;
}

static void img_tbl_settings_setup(void)
{
  char name_string[64];
  if (file_read_string("/img_tbl/boot/name",
                       name_string, sizeof(name_string)) == 0) {
    /* firmware_build_id contains the full image name */
    static char firmware_build_id[sizeof(name_string)];
    strncpy(firmware_build_id, name_string, sizeof(firmware_build_id));
    READ_ONLY_PARAMETER("system_info", "firmware_build_id",
                        firmware_build_id, TYPE_STRING);

    /* firmware_version contains everything before the git hash */
    static char firmware_version[sizeof(name_string)];
    strncpy(firmware_version, name_string, sizeof(firmware_version));
    char *sep = strstr(firmware_version, "-g");
    if (sep != NULL) {
      *sep = 0;
    }
    READ_ONLY_PARAMETER("system_info", "firmware_version",
                        firmware_version, TYPE_STRING);
  }

  char timestamp_string[32];
  if (file_read_string("/img_tbl/boot/timestamp", timestamp_string,
                       sizeof(timestamp_string)) == 0) {
    static char firmware_build_date[128];
    if (date_string_get(timestamp_string, firmware_build_date,
                        sizeof(firmware_build_date)) == 0) {
      READ_ONLY_PARAMETER("system_info", "firmware_build_date",
                          firmware_build_date, TYPE_STRING);
    }
  }

  char loader_name_string[64];
  if (file_read_string("/img_tbl/loader/name",
                       loader_name_string, sizeof(loader_name_string)) == 0) {
    static char loader_build_id[sizeof(loader_name_string)];
    strncpy(loader_build_id, loader_name_string, sizeof(loader_build_id));
    READ_ONLY_PARAMETER("system_info", "loader_build_id",
                        loader_build_id, TYPE_STRING);
  }

  char loader_timestamp_string[32];
  if (file_read_string("/img_tbl/loader/timestamp", loader_timestamp_string,
                       sizeof(loader_timestamp_string)) == 0) {
    static char loader_build_date[128];
    if (date_string_get(loader_timestamp_string, loader_build_date,
                        sizeof(loader_build_date)) == 0) {
      READ_ONLY_PARAMETER("system_info", "loader_build_date",
                          loader_build_date, TYPE_STRING);
    }
  }
}

static void reset_callback(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id; (void)context;

  /* Reset settings to defaults if requested */
  if (len == sizeof(msg_reset_t)) {
    const msg_reset_t *msg = (const void*)msg_;
    if (msg->flags & 1) {
      /* Remove settings file */
      unlink("/persistent/config.ini");
    }
  }

  /* We use -f to force immediate reboot.  Orderly shutdown sometimes fails
   * when unloading remoteproc drivers. */
  system("reboot -f");
}

int main(void)
{
  /* Ignore SIGCHLD to allow child processes to go quietly into the night
   * without turning into zombies. */
  signal(SIGCHLD, SIG_IGN);

  /* Set up SBP ZMQ */
  sbp_zmq_config_t sbp_zmq_config = {
    .sbp_sender_id = SBP_SENDER_ID,
    .pub_endpoint = ">tcp://localhost:43011",
    .sub_endpoint = ">tcp://localhost:43010"
  };
  if (sbp_zmq_init(&sbp_zmq_state, &sbp_zmq_config) != 0) {
    exit(EXIT_FAILURE);
  }

  /* Set up settings */
  settings_interface_t settings_interface = {
    .msg_send = settings_msg_send,
    .msg_cb_register = settings_msg_cb_register,
    .msg_cb_remove = settings_msg_cb_remove,
    .msg_loop_timeout = settings_msg_loop_timeout,
    .msg_loop_interrupt = settings_msg_loop_interrupt
  };
  settings_setup(&settings_interface);

  TYPE_PORT_MODE = settings_type_register_enum(port_mode_enum, &port_mode_settings_type);
  SETTING_NOTIFY("uart0", "baudrate", uart0_baudrate, TYPE_INT, baudrate_notify);
  SETTING_NOTIFY("uart1", "baudrate", uart1_baudrate, TYPE_INT, baudrate_notify);
  SETTING_NOTIFY("uart0", "mode", uart0_mode, TYPE_PORT_MODE, port_mode_notify);
  SETTING_NOTIFY("uart1", "mode", uart1_mode, TYPE_PORT_MODE, port_mode_notify);

  TYPE_IP_MODE = settings_type_register_enum(ip_mode_enum, &ip_mode_settings_type);
  SETTING_NOTIFY("ethernet", "ip_config_mode", eth_ip_mode, TYPE_IP_MODE, eth_ip_mode_notify);
  SETTING_NOTIFY("ethernet", "ip_address", eth_ip_addr, TYPE_STRING, eth_ip_config_notify);
  SETTING_NOTIFY("ethernet", "netmask", eth_netmask, TYPE_STRING, eth_ip_config_notify);
  SETTING_NOTIFY("ethernet", "gateway", eth_gateway, TYPE_STRING, eth_ip_config_notify);

  sbp_zmq_callback_register(&sbp_zmq_state, SBP_MSG_RESET,
                            reset_callback, NULL, NULL);
  sbp_zmq_callback_register(&sbp_zmq_state, SBP_MSG_RESET_DEP,
                            reset_callback, NULL, NULL);

  whitelists_init();
  img_tbl_settings_setup();
  sbp_zmq_callback_register(&sbp_zmq_state, SBP_MSG_COMMAND_REQ, sbp_command, NULL, NULL);

  sbp_zmq_loop(&sbp_zmq_state);

  sbp_zmq_deinit(&sbp_zmq_state);
  exit(EXIT_SUCCESS);
}
