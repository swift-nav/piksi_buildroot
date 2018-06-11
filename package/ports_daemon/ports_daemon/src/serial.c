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

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>

#include <libpiksi/logging.h>

#include "serial.h"

static const char * const baudrate_enum_names[] = {
  "1200", "2400", "4800", "9600",
  "19200", "38400", "57600", "115200",
  "230400", "460800", "921600", NULL
};
enum {
  BAUDRATE_1200, BAUDRATE_2400, BAUDRATE_4800, BAUDRATE_9600,
  BAUDRATE_19200, BAUDRATE_38400, BAUDRATE_57600, BAUDRATE_115200,
  BAUDRATE_230400, BAUDRATE_460800, BAUDRATE_921600
};
static const u32 baudrate_val_table[] = {
  B1200, B2400, B4800, B9600,
  B19200, B38400, B57600, B115200,
  B230400, B460800, B921600
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

int serial_init(settings_ctx_t *settings_ctx)
{
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

  return 0;
}
