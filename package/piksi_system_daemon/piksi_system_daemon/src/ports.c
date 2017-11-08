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

#include "ports.h"
#include "protocols.h"
#include "async-child.h"
#include <libpiksi/logging.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define MODE_NAME_SBP "SBP"

#define MODE_NAME_DEFAULT MODE_NAME_SBP
#define MODE_NAME_DISABLED "Disabled"
#define MODE_DISABLED 0
#define PID_INVALID 0

typedef enum {
  PORT_TYPE_UART,
  PORT_TYPE_USB,
  PORT_TYPE_TCP_SERVER,
  PORT_TYPE_TCP_CLIENT,
  PORT_TYPE_UDP_SERVER,
  PORT_TYPE_UDP_CLIENT
} port_type_t;

typedef enum {
  DO_NOT_RESTART,
  RESTART
} restart_type_t;

typedef union {
  struct {
    uint32_t port;
  } tcp_server_data;
  struct {
    char address[256];
  } tcp_client_data;
  struct {
    uint32_t port;
  } udp_server_data;
  struct {
    char address[256];
  } udp_client_data;
} opts_data_t;

typedef int (*opts_get_fn_t)(char *buf, size_t buf_size,
                             const opts_data_t *opts_data);

static int opts_get_tcp_server(char *buf, size_t buf_size,
                               const opts_data_t *opts_data)
{
  uint32_t port = opts_data->tcp_server_data.port;
  return snprintf(buf, buf_size, "--tcp-l %u", port);
}

static int opts_get_tcp_client(char *buf, size_t buf_size,
                               const opts_data_t *opts_data)
{
  const char *address = opts_data->tcp_client_data.address;
  return snprintf(buf, buf_size, "--tcp-c %s", address);
}

static int opts_get_udp_server(char *buf, size_t buf_size,
                               const opts_data_t *opts_data)
{
  uint32_t port = opts_data->udp_server_data.port;
  return snprintf(buf, buf_size, "--udp-l %u", port);
}

static int opts_get_udp_client(char *buf, size_t buf_size,
                               const opts_data_t *opts_data)
{
  const char *address = opts_data->udp_client_data.address;
  return snprintf(buf, buf_size, "--udp-c %s", address);
}

typedef struct {
  const char * const name;
  const char * const opts;
  const opts_get_fn_t opts_get;
  opts_data_t opts_data;
  const port_type_t type;
  const char * const mode_name_default;
  u8 mode;
  pid_t adapter_pid; /* May be cleared by SIGCHLD handler */
  restart_type_t restart;
} port_config_t;

static port_config_t port_configs[] = {
  {
    .name = "uart0",
    .opts = "--file /dev/ttyPS0 --nonblock --outq 8192",
    .opts_get = NULL,
    .type = PORT_TYPE_UART,
    .mode_name_default = MODE_NAME_DEFAULT,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  },
  {
    .name = "uart1",
    .opts = "--file /dev/ttyPS1 --nonblock --outq 8192",
    .opts_get = NULL,
    .type = PORT_TYPE_UART,
    .mode_name_default = MODE_NAME_DEFAULT,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  },
  {
    .name = "usb0",
    .opts = "--file /dev/ttyGS0 --nonblock --outq 8192",
    .opts_get = NULL,
    .type = PORT_TYPE_USB,
    .mode_name_default = MODE_NAME_DEFAULT,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = RESTART
  },
  {
    .name = "tcp_server0",
    .opts = "",
    .opts_data.tcp_server_data.port = 55555,
    .opts_get = opts_get_tcp_server,
    .type = PORT_TYPE_TCP_SERVER,
    .mode_name_default = MODE_NAME_DEFAULT,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  },
  {
    .name = "tcp_server1",
    .opts = "",
    .opts_data.tcp_server_data.port = 55556,
    .opts_get = opts_get_tcp_server,
    .type = PORT_TYPE_TCP_SERVER,
    .mode_name_default = MODE_NAME_DEFAULT,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  },
  {
    .name = "tcp_client0",
    .opts = "",
    .opts_data.tcp_client_data.address = "",
    .opts_get = opts_get_tcp_client,
    .type = PORT_TYPE_TCP_CLIENT,
    .mode_name_default = MODE_NAME_DISABLED,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  },
  {
    .name = "tcp_client1",
    .opts = "",
    .opts_data.tcp_client_data.address = "",
    .opts_get = opts_get_tcp_client,
    .type = PORT_TYPE_TCP_CLIENT,
    .mode_name_default = MODE_NAME_DISABLED,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  },
  {
    .name = "udp_server0",
    .opts = "",
    .opts_data.udp_server_data.port = 55557,
    .opts_get = opts_get_udp_server,
    .type = PORT_TYPE_UDP_SERVER,
    .mode_name_default = MODE_NAME_DEFAULT,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  },
  {
    .name = "udp_server1",
    .opts = "",
    .opts_data.udp_server_data.port = 55558,
    .opts_get = opts_get_udp_server,
    .type = PORT_TYPE_UDP_SERVER,
    .mode_name_default = MODE_NAME_DEFAULT,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  },
  {
    .name = "udp_client0",
    .opts = "",
    .opts_data.udp_client_data.address = "",
    .opts_get = opts_get_udp_client,
    .type = PORT_TYPE_UDP_CLIENT,
    .mode_name_default = MODE_NAME_DISABLED,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  },
  {
    .name = "udp_client1",
    .opts = "",
    .opts_data.udp_client_data.address = "",
    .opts_get = opts_get_udp_client,
    .type = PORT_TYPE_UDP_CLIENT,
    .mode_name_default = MODE_NAME_DISABLED,
    .mode = MODE_DISABLED,
    .adapter_pid = PID_INVALID,
    .restart = DO_NOT_RESTART
  }
};

static int mode_to_protocol_index(u8 mode)
{
  return mode - 1;
}

static u8 protocol_index_to_mode(int protocol_index)
{
  return protocol_index + 1;
}

static void adapter_kill(port_config_t *port_config)
{
  /* Mask SIGCHLD while accessing adapter_pid */
  sigset_t saved_mask;
  sigchld_mask(&saved_mask);
  {
    if (port_config->adapter_pid > 0) {
      int ret = kill(port_config->adapter_pid, SIGTERM);
      piksi_log(LOG_DEBUG,
                "Killing zmq_adapter with PID: %d (kill returned %d, errno %d)",
                port_config->adapter_pid, ret, errno);
    }
    port_config->adapter_pid = 0;
  }
  sigchld_restore(&saved_mask);
}

static int port_configure(port_config_t *port_config)
{
  /* kill adapter */
  adapter_kill(port_config);

  /* In case of USB adapters, sometimes we find that instances are still around.
   * Kill them here
   */
  if (port_config->type == PORT_TYPE_USB) {
    system("kill -9 `ps | grep GS0  | grep zmq_adapter | awk -F' ' '{print $1}'`");
  }

  if (port_config->mode == MODE_DISABLED) {
    return 0;
  }

  int protocol_index = mode_to_protocol_index(port_config->mode);
  const protocol_t *protocol = protocols_get(protocol_index);
  if (protocol == NULL) {
    return -1;
  }

  /* Prepare the command used to launch zmq_adapter. */
  char mode_opts[256] = {0};
  protocol->port_adapter_opts_get(mode_opts, sizeof(mode_opts),
                                  port_config->name);

  char opts[256] = {0};
  if (port_config->opts_get != NULL) {
    port_config->opts_get(opts, sizeof(opts), &port_config->opts_data);
  }

  char cmd[512];
  snprintf(cmd, sizeof(cmd),
           "zmq_adapter %s %s %s",
           port_config->opts, opts, mode_opts);

  /* Split the command on each space for argv */
  char *args[32] = {0};
  args[0] = strtok(cmd, " ");
  for (u8 i=1; (args[i] = strtok(NULL, " ")) && i < 32; i++);

  piksi_log(LOG_DEBUG, "Starting zmq_adapter: %s", cmd);

  /* Mask SIGCHLD while accessing adapter_pid */
  sigset_t saved_mask;
  sigchld_mask(&saved_mask);
  {
    /* Create a new zmq_adapter. */
    if (!(port_config->adapter_pid = fork())) {
      execvp(args[0], args);
      piksi_log(LOG_ERR, "execvp error");
      exit(EXIT_FAILURE);
    }

    piksi_log(LOG_DEBUG, "zmq_adapter started with PID: %d",
              port_config->adapter_pid);
  }
  sigchld_restore(&saved_mask);

  return 0;
}

static int setting_mode_notify(void *context)
{
  port_config_t *port_config = (port_config_t *)context;
  return port_configure(port_config);
}

static int setting_tcp_server_port_notify(void *context)
{
  port_config_t *port_config = (port_config_t *)context;
  return port_configure(port_config);
}

static int setting_tcp_client_address_notify(void *context)
{
  port_config_t *port_config = (port_config_t *)context;
  return port_configure(port_config);
}

static int setting_udp_server_port_notify(void *context)
{
  port_config_t *port_config = (port_config_t *)context;
  return port_configure(port_config);
}

static int setting_udp_client_address_notify(void *context)
{
  port_config_t *port_config = (port_config_t *)context;
  return port_configure(port_config);
}

static int setting_mode_register(settings_ctx_t *settings_ctx,
                                 settings_type_t settings_type,
                                 port_config_t *port_config)
{
  return settings_register(settings_ctx, port_config->name, "mode",
                           &port_config->mode, sizeof(port_config->mode),
                           settings_type, setting_mode_notify,
                           port_config);
}

static int setting_tcp_server_port_register(settings_ctx_t *settings_ctx,
                                            port_config_t *port_config)
{
  return settings_register(settings_ctx, port_config->name, "port",
                           &port_config->opts_data.tcp_server_data.port,
                           sizeof(&port_config->opts_data.tcp_server_data.port),
                           SETTINGS_TYPE_INT, setting_tcp_server_port_notify,
                           port_config);
}

static int setting_tcp_client_address_register(settings_ctx_t *settings_ctx,
                                               port_config_t *port_config)
{
  return settings_register(settings_ctx, port_config->name, "address",
                           port_config->opts_data.tcp_client_data.address,
                           sizeof(port_config->opts_data.tcp_client_data.address),
                           SETTINGS_TYPE_STRING, setting_tcp_client_address_notify,
                           port_config);
}

static int setting_udp_server_port_register(settings_ctx_t *settings_ctx,
                                            port_config_t *port_config)
{
  return settings_register(settings_ctx, port_config->name, "port",
                           &port_config->opts_data.udp_server_data.port,
                           sizeof(&port_config->opts_data.udp_server_data.port),
                           SETTINGS_TYPE_INT, setting_udp_server_port_notify,
                           port_config);
}

static int setting_udp_client_address_register(settings_ctx_t *settings_ctx,
                                               port_config_t *port_config)
{
  return settings_register(settings_ctx, port_config->name, "address",
                           port_config->opts_data.udp_client_data.address,
                           sizeof(port_config->opts_data.udp_client_data.address),
                           SETTINGS_TYPE_STRING, setting_udp_client_address_notify,
                           port_config);
}

static int mode_enum_names_get(const char ***mode_enum_names)
{
  int protocols_count = protocols_count_get();

  const char **enum_names =
      (const char **)malloc((1 + protocol_index_to_mode(protocols_count)) *
                            sizeof(*enum_names));
  if (enum_names == NULL) {
    return -1;
  }

  enum_names[MODE_DISABLED] = MODE_NAME_DISABLED;

  for (int i = 0; i < protocols_count; i++) {
    const protocol_t *protocol = protocols_get(i);
    assert(protocol != NULL);
    enum_names[protocol_index_to_mode(i)] = protocol->setting_name;
  }
  enum_names[protocol_index_to_mode(protocols_count)] = NULL;

  *mode_enum_names = enum_names;
  return 0;
}

static int mode_lookup(const char *mode_name, u8 *mode)
{
  if (strcasecmp(mode_name, MODE_NAME_DISABLED) == 0) {
    *mode = MODE_DISABLED;
    return 0;
  }

  int protocols_count = protocols_count_get();

  for (int i = 0; i < protocols_count; i++) {
    const protocol_t *protocol = protocols_get(i);
    assert(protocol != NULL);
    if (strcasecmp(protocol->setting_name, mode_name) == 0) {
      *mode = protocol_index_to_mode(i);
      return 0;
    }
  }

  piksi_log(LOG_WARNING, "failed to look up port mode: %s", mode_name);
  return -1;
}

int ports_init(settings_ctx_t *settings_ctx)
{
  int i;

  /* Initialize default mode */
  u8 mode_default = MODE_DISABLED;
  mode_lookup(MODE_NAME_DEFAULT, &mode_default);

  /* Initialize port modes */
  for (i = 0; i < sizeof(port_configs) / sizeof(port_configs[0]); i++) {
    port_config_t *port_config = &port_configs[i];

    if (mode_lookup(port_config->mode_name_default, &port_config->mode) != 0) {
      port_config->mode = mode_default;
    }
  }

  /* Get mode enum names */
  const char **mode_enum_names;
  if (mode_enum_names_get(&mode_enum_names) != 0) {
    piksi_log(LOG_ERR, "error setting up port modes");
    return -1;
  }

  /* Register settings types */
  settings_type_t settings_type_mode;
  settings_type_register_enum(settings_ctx, mode_enum_names,
                              &settings_type_mode);

  /* Register settings */
  for (i = 0; i < sizeof(port_configs) / sizeof(port_configs[0]); i++) {
    port_config_t *port_config = &port_configs[i];

    setting_mode_register(settings_ctx, settings_type_mode, port_config);

    if (port_config->type == PORT_TYPE_TCP_SERVER) {
      setting_tcp_server_port_register(settings_ctx, port_config);
    }

    if (port_config->type == PORT_TYPE_TCP_CLIENT) {
      setting_tcp_client_address_register(settings_ctx, port_config);
    }

    if (port_config->type == PORT_TYPE_UDP_SERVER) {
      setting_udp_server_port_register(settings_ctx, port_config);
    }

    if (port_config->type == PORT_TYPE_UDP_CLIENT) {
      setting_udp_client_address_register(settings_ctx, port_config);
    }
  }

  return 0;
}

void ports_sigchld_waitpid_handler(pid_t pid, int status)
{
  int i;
  for (i = 0; i < sizeof(port_configs) / sizeof(port_configs[0]); i++) {
    port_config_t *port_config = &port_configs[i];

    if (port_config->adapter_pid == pid) {
      port_config->adapter_pid = 0;
      fprintf(stdout, "Adapter %s died\n", port_config->name);
      if (port_config->restart == RESTART) {
        port_configure(port_config);
      }
    }
  }
}
