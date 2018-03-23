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

#include "at_command_utils.h"

#define AT_COMMAND_BUFFER_STACK_MAX (128u)
#define AT_COMMAND_PREFIX "AT"
#define AT_COMMAND_CSQ "+CSQ"

#define SERIAL_PORT_NAME_MAX_LENGTH (128u)
#define AT_COMMAND_MAX_LENGTH (128u)
#define AT_RESULT_MAX_LENGTH (128u)

struct at_serial_port_s {
  char port_name[SERIAL_PORT_NAME_MAX_LENGTH];
  int fd;
  bool is_open;
};

struct at_serial_port_command_s {
  char command[AT_COMMAND_MAX_LENGTH];
  char result[AT_RESULT_MAX_LENGTH];
  size_t result_length;
  bool complete;
};

// Ref: Serial Port Programming - https://www.cmrr.umn.edu/~strupp/serial.html
/*
 * 'open_port()' - Open serial port 1.
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

  fd = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
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

// Ref: Standard Port Config for Serial Modem -
// https://www.cmrr.umn.edu/~strupp/serial.html
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

at_serial_port_t *at_serial_port_create(const char *port_name)
{
  at_serial_port_t *port = NULL;
  if (port_name != NULL && port_name[0] != '\0'
      && strlen(port_name) < sizeof(port->port_name)) {
    port = (at_serial_port_t *)malloc(sizeof(struct at_serial_port_s));
    if (port != NULL) {
      memset(port, 0, sizeof(struct at_serial_port_s));
      strcpy(port->port_name, port_name);
      port->fd = -1;
      port->is_open = false;
    }
  }
  return port;
}

void at_serial_port_open(at_serial_port_t *port)
{
  if (!at_serial_port_is_open(port)) {
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

void at_serial_port_close(at_serial_port_t *port)
{
  if (at_serial_port_is_open(port)) {
    close(port->fd);
    port->fd = -1;
    port->is_open = false;
  }
}

bool at_serial_port_is_open(at_serial_port_t *port)
{
  return port->is_open;
}

void at_serial_port_destroy(at_serial_port_t **port_loc)
{
  at_serial_port_t *port = *port_loc;
  if (port_loc != NULL && port != NULL) {
    at_serial_port_close(port);
    free(port);
    *port_loc = NULL;
  }
}

at_serial_port_command_t *at_serial_port_command_create(const char *command)
{
  at_serial_port_command_t *at_command = NULL;
  if (command != NULL && command[0] != '\0'
      && strlen(command) < sizeof(at_command->command)) {
    at_command = (at_serial_port_command_t *)malloc(
      sizeof(struct at_serial_port_command_s));
    if (at_command != NULL) {
      memset(at_command, 0, sizeof(struct at_serial_port_command_s));
      strcpy(at_command->command, command);
      at_command->complete = false;
    }
  }
  return at_command;
}

void at_serial_port_command_destroy(at_serial_port_command_t **at_command_loc)
{
  at_serial_port_command_t *at_command = *at_command_loc;
  if (at_command_loc != NULL && at_command != NULL) {
    free(at_command);
    *at_command_loc = NULL;
  }
}

static void
at_serial_port_wait_command_response(at_serial_port_t *port,
                                     at_serial_port_command_t *at_command)
{
  char *sob = at_command->result;
  char *current = sob;
  char *eob = sob + sizeof(at_command->result);
  int num_read = 0;

  /* read characters into our string buffer until we get a CR or NL */
  while ((num_read = (int)read(port->fd, current, (size_t)(eob - current) - 1))
         > 0) {
    current += num_read;
    if (current[-1] == '\n' || current[-1] == '\r') {
      /* null terminate the string and see if we got an OK response */
      *current = '\0';
      if (current - sob >= 4 && strstr(current - 4, "OK") != NULL) {
        at_command->complete = true;
      }
      if (current - sob >= 7 && strstr(current - 7, "ERROR") != NULL) {
        at_command->complete = true;
      }
    }
    if (at_command->complete || current == eob - 1) {
      break;
    }
  }

  *current = '\0';
  at_command->result_length = (size_t)(current - at_command->result);
}

int at_serial_port_execute_command(at_serial_port_t *port,
                                   at_serial_port_command_t *at_command)
{
  int ret = 0;
  at_serial_port_open(port);
  if (!at_serial_port_is_open(port)) {
    piksi_log(LOG_ERR, "Unable to open port: %s", port->port_name);
    return -1;
  }

  int n = (int)write(port->fd, at_command->command, strlen(at_command->command));
  n += (int)write(port->fd, "\r", 1);

  if (n != (int)strlen(at_command->command) + 1) {
    piksi_log(LOG_ERR,
        "Unable to write command '%s' to port '%s'",
        at_command->command,
        port->port_name);
    ret = -1;
    goto cleanup;
  }

  at_serial_port_wait_command_response(port, at_command);
  if (!at_command->complete) {
    piksi_log(
        LOG_ERR,
        "Unable to resolve result of command '%s' to port '%s'. %d characters returned",
        at_command->command,
        port->port_name,
        at_command->result_length);
    ret = -1;
    goto cleanup;
  }

cleanup:
  at_serial_port_close(port);
  return ret;
}

const char *at_serial_port_command_result(at_serial_port_command_t *at_command)
{
  return at_command->result;
}

/* The following from Telit HE910 AT Command Reference
 * +CSQ: <rssi>,<ber>
 * where
 * <rssi> - received signal strength indication
 *  0 - (-113) dBm or less
 *  1 - (-111) dBm
 *  2..30 - (-109)dBm..(-53)dBm / 2 dBm per step
 *  31 - (-51)dBm or greater
 *  99 - not known or not detectable
 * <ber> - bit error rate (in percent)
 *  0 - less than 0.2%
 *  1 - 0.2% to 0.4%
 *  2 - 0.4% to 0.8%
 *  3 - 0.8% to 1.6%
 *  4 - 1.6% to 3.2%
 *  5 - 3.2% to 6.4%
 *  6 - 6.4% to 12.8%
 *  7 - more than 12.8%
 *  99 - not known or not detectable
 */
static const s8 signal_to_dbm_lookup[] = {
  -113, // zero case
  -111, -109, -107, -105, -103, -101, -99, -97, -95, -93,  // 1 - 30 cases
  -91,  -89,  -87,  -85,  -83,  -81,  -79,  -77, -75, -73,
  -71, -69,  -67,  -65,  -63,  -61,  -59,  -57,  -55, -53,
  -51 // 31 case
};

static s8 modem_signal_strength_to_dbm(u32 signal_raw)
{
  s8 signal_dbm = 0; // unknown will remain 0

  if (signal_raw < (s8)COUNT_OF(signal_to_dbm_lookup)) {
    signal_dbm = signal_to_dbm_lookup[signal_raw];
  }
  return signal_dbm;
}

// Splits the difference from above
static const float error_to_percent_lookup[] = {
  0.2f, 0.3f, 0.6f, 1.2f, 2.4f, 4.8f, 9.6f, 12.8f
};

static float modem_error_rate_to_percent(u32 error_raw)
{
  float error_percent = 0.0; // unknown will remain 0

  if (error_raw < (u32)COUNT_OF(error_to_percent_lookup)) {
    error_percent = error_to_percent_lookup[error_raw];
  }
  return error_percent;
}

// This block should be refactored
int at_command_report_signal_quality(at_serial_port_t *port,
                                     s8 *signal_strength,
                                     float *error_rate)
{
  int ret = 0;
  const char *at_prefix = AT_COMMAND_PREFIX;
  const char *base_command = AT_COMMAND_CSQ;
  char command_buffer[AT_COMMAND_BUFFER_STACK_MAX];
  snprintf(command_buffer,
      sizeof(command_buffer),
      "%s%s",
      at_prefix,
      base_command);

  at_serial_port_command_t *at_command =
    at_serial_port_command_create(command_buffer);
  if (at_command == NULL) {
    return -1;
  }
  at_serial_port_execute_command(port, at_command);
  const char *result = at_serial_port_command_result(at_command);

  snprintf(command_buffer, sizeof(command_buffer), "%s:", base_command);
  char *parse_ref = strstr(result, command_buffer);
  if (parse_ref == NULL) {
    ret = -1;
    goto cleanup;
  }

  parse_ref += strlen(command_buffer);
  char *check_pos = NULL;
  errno = 0;
  u32 signal_raw = (u32)strtoul(parse_ref, &check_pos, 10);
  if (errno != 0 || check_pos == parse_ref) {
    ret = -1;
    goto cleanup;
  } else {
    parse_ref = check_pos;
    *signal_strength = modem_signal_strength_to_dbm(signal_raw);
  }

  if (*parse_ref++ != ',') {
    ret = -1;
    goto cleanup;
  }

  u32 error_raw = (u32)strtoul(parse_ref, &check_pos, 10);
  if (errno != 0 || check_pos == parse_ref) {
    ret = -1;
    goto cleanup;
  } else {
    *error_rate = modem_error_rate_to_percent(error_raw);
  }

cleanup:
  at_serial_port_command_destroy(&at_command);
  return ret;
}
