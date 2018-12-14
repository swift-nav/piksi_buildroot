/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include <libpiksi/serial_utils.h>

// Ref: Serial Port Programming - https://www.cmrr.umn.edu/~strupp/serial.html
// Ref: Piksi Buildroot Cell Modem Daemon
// TODO: someday add serial API to libpiksi
/*
 * 'open_port()' - Open serial port at descriptor port_name.
 *
 * Returns the file descriptor on success or -1 on error.
 */
static int open_port(const char *port_name)
{
  int fd; /* File descriptor for the port */

  if (port_name == NULL) {
    return -1;
  }

  if (port_name[0] == '\0') {
    return -1;
  }

  fd = open(port_name, O_RDWR | O_NONBLOCK);
  if (fd == -1) {
    /*
     * Could not open the port.
     */

    piksi_log(LOG_ERR, "Unable to open port: %s", port_name);
  } else {
    fcntl(fd, F_SETFL, 0);
  }

  return (fd);
}


static void configure_port(int fd)
{
  struct termios options;

  /* get the current options */
  tcgetattr(fd, &options);

  /* set raw input, 1 second timeout */
  options.c_cflag |= (CLOCAL | CREAD);
  options.c_lflag &= (unsigned)~(ICANON | ECHO | ECHOE | ISIG);
  options.c_oflag &= (unsigned)~OPOST;
  options.c_cc[VMIN] = 0;
  options.c_cc[VTIME] = 10;

  /* set the options */
  tcsetattr(fd, TCSANOW, &options);
}

// Ref: Standard Port Config for Serial Modem -
// https://www.cmrr.umn.edu/~strupp/serial.html

static int configure_port_baud(int fd, speed_t speed)
{
  struct termios options;

  configure_port(fd);

  /* get the current options */
  tcgetattr(fd, &options);

  /* set the baud */
  if (cfsetispeed(&options, speed) != 0) {
    fprintf(stderr, "cfsetispeed failed: %s\n", strerror(errno));
    return 1;
  }
  if (cfsetospeed(&options, speed) != 0) {
    fprintf(stderr, "cfsetospeed failed: %s\n", strerror(errno));
    return 1;
  }

  /* set the options */
  if (tcsetattr(fd, TCSANOW, &options) != 0) {
    fprintf(stderr, "cfsetispeed failed: %s\n", strerror(errno));
    return 1;
  }
  return 0;
}

serial_port_t *serial_port_create(const char *port_name)
{
  serial_port_t *port = NULL;
  if (port_name != NULL && port_name[0] != '\0' && strlen(port_name) < sizeof(port->port_name)) {
    port = (serial_port_t *)malloc(sizeof(struct serial_port_s));
    if (port != NULL) {
      memset(port, 0, sizeof(struct serial_port_s));
      strcpy(port->port_name, port_name);
      port->fd = -1;
      port->is_open = false;
    }
  }
  return port;
}

void serial_port_open_baud(serial_port_t *port, speed_t speed)
{
  if (!serial_port_is_open(port)) {
    int fd = open_port(port->port_name);
    if (fd == -1) {
      // failed to open port
    } else {
      configure_port_baud(fd, speed);
      port->fd = fd;
      port->is_open = true;
    }
  }
}

void serial_port_open(serial_port_t *port)
{
  if (!serial_port_is_open(port)) {
    int fd = open_port(port->port_name);
    if (fd == -1) {
      // failed to open port
    } else {
      configure_port(fd);
      port->fd = fd;
      port->is_open = true;
    }
  }
}

void serial_port_close(serial_port_t *port)
{
  if (serial_port_is_open(port)) {
    close(port->fd);
    port->fd = -1;
    port->is_open = false;
  }
}

bool serial_port_is_open(serial_port_t *port)
{
  return port->is_open;
}

void serial_port_destroy(serial_port_t **port_loc)
{
  serial_port_t *port = *port_loc;
  if (port_loc != NULL && port != NULL) {
    serial_port_close(port);
    free(port);
    *port_loc = NULL;
  }
}
