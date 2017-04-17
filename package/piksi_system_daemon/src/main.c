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

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/settings.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libsbp/piksi.h>
#include <libsbp/logging.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <termios.h>

#include "skylark.h"
#include "whitelists.h"

#define PROGRAM_NAME "piksi_system_daemon"

#define PUB_ENDPOINT ">tcp://127.0.0.1:43011"
#define SUB_ENDPOINT ">tcp://127.0.0.1:43010"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

static void sigchld_handler(int signum)
{
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0) {
    ;
  }
  errno = saved_errno;
}

static const char * const baudrate_enum_names[] = {
  "1200", "2400", "4800", "9600",
  "19200", "38400", "57600", "115200",
  "230400", NULL
};
enum {
  BAUDRATE_1200, BAUDRATE_2400, BAUDRATE_4800, BAUDRATE_9600,
  BAUDRATE_19200, BAUDRATE_38400, BAUDRATE_57600, BAUDRATE_115200,
  BAUDRATE_230400
};
static const u32 baudrate_val_table[] = {
  B1200, B2400, B4800, B9600,
  B19200, B38400, B57600, B115200,
  B230400
};

static const char * const flow_control_enum_names[] = {"None", "RTS/CTS", NULL};
enum {FLOW_CONTROL_NONE, FLOW_CONTROL_RTS_CTS};

typedef struct {
  const char *tty_path;
  u8 baudrate;
  u8 flow_control;
} uart_t;

static uart_t uart0 = {
  .tty_path = "/dev/ttyPS0",
  .baudrate = BAUDRATE_115200,
  .flow_control = FLOW_CONTROL_NONE
};

static uart_t uart1 = {
  .tty_path = "/dev/ttyPS1",
  .baudrate = BAUDRATE_115200,
  .flow_control = FLOW_CONTROL_NONE
};

static uart_t usb0 = {
  .tty_path = "/dev/ttyGS0",
  .baudrate = BAUDRATE_9600,
  .flow_control = FLOW_CONTROL_NONE
};

static int uart_configure(const uart_t *uart)
{
  int fd = open(uart->tty_path, O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    piksi_log(LOG_ERR, "error opening tty device");
    return -1;
  }

  struct termios tio;
  if (tcgetattr(fd, &tio) != 0) {
    piksi_log(LOG_ERR, "error in tcgetattr()");
    close(fd);
    return -1;
  }

  cfmakeraw(&tio);
  tio.c_lflag &= ~ECHO;
  tio.c_oflag &= ~ONLCR;
  tio.c_cflag = ((tio.c_cflag & ~CRTSCTS) |
                 (uart->flow_control == FLOW_CONTROL_RTS_CTS ? CRTSCTS : 0));
  cfsetispeed(&tio, baudrate_val_table[uart->baudrate]);
  cfsetospeed(&tio, baudrate_val_table[uart->baudrate]);
  tcsetattr(fd, TCSANOW, &tio);

  /* Check results */
  if (tcgetattr(fd, &tio) != 0) {
    piksi_log(LOG_ERR, "error in tcgetattr()");
    close(fd);
    return -1;
  }

  close(fd);

  if ((cfgetispeed(&tio) != baudrate_val_table[uart->baudrate]) ||
      (cfgetospeed(&tio) != baudrate_val_table[uart->baudrate]) ||
      ((tio.c_cflag & CRTSCTS) ? (uart->flow_control != FLOW_CONTROL_RTS_CTS) :
                                 (uart->flow_control != FLOW_CONTROL_NONE))) {
    piksi_log(LOG_ERR, "error configuring tty");
    return -1;
  }

  return 0;
}

static int baudrate_notify(void *context)
{
  const uart_t *uart = (uart_t *)context;
  return uart_configure(uart);
}

static int flow_control_notify(void *context)
{
  const uart_t *uart = (uart_t *)context;
  return uart_configure(uart);
}

static const char const * port_mode_enum_names[] = {
  "SBP", "NMEA", "RTCM3 IN", NULL
};
enum {
  PORT_MODE_SBP, PORT_MODE_NMEA, PORT_MODE_RTCM3_IN
};

typedef struct {
  const char * const name;
  const char * const opts;
  u8 mode;
  pid_t pid;
} adapter_config_t;

static adapter_config_t uart0_adapter_config = {
  .name = "uart0",
  .opts = "--file /dev/ttyPS0",
  .mode = PORT_MODE_SBP,
  .pid = 0
};

static adapter_config_t uart1_adapter_config = {
  .name = "uart1",
  .opts = "--file /dev/ttyPS1",
  .mode = PORT_MODE_SBP,
  .pid = 0
};

static adapter_config_t usb0_adapter_config = {
  .name = "usb0",
  .opts = "--file /dev/ttyGS0",
  .mode = PORT_MODE_SBP,
  .pid = 0
};

static adapter_config_t tcp_server0_adapter_config = {
  .name = "tcp_server0",
  .opts = "--tcp-l 55555",
  .mode = PORT_MODE_SBP,
  .pid = 0
};

static adapter_config_t tcp_server1_adapter_config = {
  .name = "tcp_server1",
  .opts = "--tcp-l 55556",
  .mode = PORT_MODE_SBP,
  .pid = 0
};

int port_mode_notify(void *context)
{
  adapter_config_t *adapter_config = (adapter_config_t *)context;

  char mode_opts[200] = {0};
  u16 zmq_port_pub = 0;
  u16 zmq_port_sub = 0;
  switch (adapter_config->mode) {
  case PORT_MODE_SBP:
    snprintf(mode_opts, sizeof(mode_opts),
             "-f sbp --filter-out sbp "
             "--filter-out-config /etc/%s_filter_out_config",
             adapter_config->name);
    zmq_port_pub = 43031;
    zmq_port_sub = 43030;
    break;
  case PORT_MODE_NMEA:
    snprintf(mode_opts, sizeof(mode_opts), "");
    zmq_port_pub = 44031;
    zmq_port_sub = 44030;
    break;
  case PORT_MODE_RTCM3_IN:
    snprintf(mode_opts, sizeof(mode_opts), "-f rtcm3");
    zmq_port_pub = 45031;
    zmq_port_sub = 45030;
    break;
  default:
    return -1;
  }

  /* Kill the old zmq_adapter, if it exists. */
  if (adapter_config->pid) {
    int ret = kill(adapter_config->pid, SIGTERM);
    piksi_log(LOG_DEBUG,
              "Killing zmq_adapter with PID: %d (kill returned %d, errno %d)",
              adapter_config->pid, ret, errno);
  }

  /* Prepare the command used to launch zmq_adapter. */
  char cmd[200];
  snprintf(cmd, sizeof(cmd),
           "zmq_adapter %s %s "
           "-p >tcp://127.0.0.1:%d "
           "-s >tcp://127.0.0.1:%d",
           adapter_config->opts, mode_opts, zmq_port_pub, zmq_port_sub);

  piksi_log(LOG_DEBUG, "Starting zmq_adapter: %s", cmd);

  /* Split the command on each space for argv */
  char *args[100] = {0};
  args[0] = strtok(cmd, " ");
  for (u8 i=1; (args[i] = strtok(NULL, " ")) && i < 100; i++);

  /* Create a new zmq_adapter. */
  if (!(adapter_config->pid = fork())) {
    execvp(args[0], args);
    piksi_log(LOG_ERR, "execvp error");
    exit(EXIT_FAILURE);
  }

  piksi_log(LOG_DEBUG, "zmq_adapter started with PID: %d",
            adapter_config->pid);

  if (adapter_config->pid < 0) {
    /* fork() failed */
    return -1;
  }

  return 0;
}

static const char const * ip_mode_enum_names[] = {"Static", "DHCP", NULL};
enum {IP_CFG_STATIC, IP_CFG_DHCP};
static u8 eth_ip_mode = IP_CFG_STATIC;
static char eth_ip_addr[16] = "192.168.0.222";
static char eth_netmask[16] = "255.255.255.0";
static char eth_gateway[16] = "192.168.0.1";

static void eth_update_config(void)
{
  system("ifdown -f eth0");

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

struct shell_cmd_ctx {
  FILE *pipe;
  u32 sequence;
  sbp_zmq_pubsub_ctx_t *pubsub_ctx;
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
    sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(ctx->pubsub_ctx),
                    SBP_MSG_LOG, sizeof(*msg) + strlen(msg->text), (void*)msg);
  } else {
    /* Broken pipe, clean up and send exit code */
    msg_command_resp_t resp = {
      .sequence = ctx->sequence,
      .code = pclose(ctx->pipe),
    };
    sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(ctx->pubsub_ctx),
                    SBP_MSG_COMMAND_RESP, sizeof(resp), (void*)&resp);
    zloop_poller_end(sbp_zmq_pubsub_zloop_get(ctx->pubsub_ctx), item);
    free(ctx);
  }
  return 0;
}

static void sbp_command(u16 sender_id, u8 len, u8 msg_[], void* context)
{
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = (sbp_zmq_pubsub_ctx_t *)context;

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
    sbp_zmq_tx_send(sbp_zmq_pubsub_tx_ctx_get(pubsub_ctx),
                    SBP_MSG_COMMAND_RESP, sizeof(resp), (u8*)&resp);
    return;
  }
  struct shell_cmd_ctx *ctx = calloc(1, sizeof(*ctx));
  ctx->pipe = popen(msg->command, "r");
  ctx->sequence = msg->sequence;
  ctx->pubsub_ctx = pubsub_ctx;
  ctx->pollitem.fd = fileno(ctx->pipe);
  ctx->pollitem.events = ZMQ_POLLIN;
  int arg = O_NONBLOCK;
  fcntl(fileno(ctx->pipe), F_SETFL, &arg);
  zloop_poller(sbp_zmq_pubsub_zloop_get(ctx->pubsub_ctx),
               &ctx->pollitem, command_output, ctx);
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
  logging_init(PROGRAM_NAME);

  /* Set up SIGCHLD handler */
  struct sigaction sigchld_sa;
  sigchld_sa.sa_handler = sigchld_handler;
  sigemptyset(&sigchld_sa.sa_mask);
  sigchld_sa.sa_flags = SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sigchld_sa, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting up sigchld handler");
    exit(EXIT_FAILURE);
  }

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

  /* Configure USB0 */
  uart_configure(&usb0);

  /* Register settings */
  settings_type_t settings_type_baudrate;
  settings_type_register_enum(settings_ctx, baudrate_enum_names,
                              &settings_type_baudrate);
  settings_register(settings_ctx, "uart0", "baudrate", &uart0.baudrate,
                    sizeof(uart0.baudrate), settings_type_baudrate,
                    baudrate_notify, &uart0);
  settings_register(settings_ctx, "uart1", "baudrate", &uart1.baudrate,
                    sizeof(uart1.baudrate), settings_type_baudrate,
                    baudrate_notify, &uart1);

  settings_type_t settings_type_flow_control;
  settings_type_register_enum(settings_ctx, flow_control_enum_names,
                              &settings_type_flow_control);
  settings_register(settings_ctx, "uart0", "flow_control", &uart0.flow_control,
                    sizeof(uart0.flow_control), settings_type_flow_control,
                    flow_control_notify, &uart0);
  settings_register(settings_ctx, "uart1", "flow_control", &uart1.flow_control,
                    sizeof(uart1.flow_control), settings_type_flow_control,
                    flow_control_notify, &uart1);

  settings_type_t settings_type_port_mode;
  settings_type_register_enum(settings_ctx, port_mode_enum_names,
                              &settings_type_port_mode);
  settings_register(settings_ctx, "uart0", "mode", &uart0_adapter_config.mode,
                    sizeof(uart0_adapter_config.mode), settings_type_port_mode,
                    port_mode_notify, &uart0_adapter_config);
  settings_register(settings_ctx, "uart1", "mode", &uart1_adapter_config.mode,
                    sizeof(uart1_adapter_config.mode), settings_type_port_mode,
                    port_mode_notify, &uart1_adapter_config);
  settings_register(settings_ctx, "usb0", "mode", &usb0_adapter_config.mode,
                    sizeof(usb0_adapter_config.mode), settings_type_port_mode,
                    port_mode_notify, &usb0_adapter_config);
  settings_register(settings_ctx, "tcp_server0", "mode", &tcp_server0_adapter_config.mode,
                    sizeof(tcp_server0_adapter_config.mode), settings_type_port_mode,
                    port_mode_notify, &tcp_server0_adapter_config);
  settings_register(settings_ctx, "tcp_server1", "mode", &tcp_server1_adapter_config.mode,
                    sizeof(tcp_server1_adapter_config.mode), settings_type_port_mode,
                    port_mode_notify, &tcp_server1_adapter_config);

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

  sbp_zmq_rx_callback_register(sbp_zmq_pubsub_rx_ctx_get(pubsub_ctx),
                               SBP_MSG_RESET, reset_callback, NULL, NULL);
  sbp_zmq_rx_callback_register(sbp_zmq_pubsub_rx_ctx_get(pubsub_ctx),
                               SBP_MSG_RESET_DEP, reset_callback, NULL, NULL);

  skylark_init(settings_ctx);
  whitelists_init(settings_ctx);
  img_tbl_settings_setup(settings_ctx);
  sbp_zmq_rx_callback_register(sbp_zmq_pubsub_rx_ctx_get(pubsub_ctx),
                               SBP_MSG_COMMAND_REQ, sbp_command, pubsub_ctx, NULL);

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(pubsub_ctx));

  sbp_zmq_pubsub_destroy(&pubsub_ctx);
  settings_destroy(&settings_ctx);
  exit(EXIT_SUCCESS);
}
