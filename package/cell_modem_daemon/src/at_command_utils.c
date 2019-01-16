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

#include "at_command_utils.h"

#define AT_COMMAND_BUFFER_STACK_MAX (128u)
#define AT_COMMAND_PREFIX "AT"
#define AT_COMMAND_CSQ "+CSQ"

#define AT_COMMAND_MAX_LENGTH (128u)
#define AT_RESULT_MAX_LENGTH (128u)

struct at_serial_port_command_s {
  char command[AT_COMMAND_MAX_LENGTH];
  char result[AT_RESULT_MAX_LENGTH];
  size_t result_length;
  bool complete;
};

at_serial_port_command_t *at_serial_port_command_create(const char *command)
{
  at_serial_port_command_t *at_command = NULL;
  if (command != NULL && command[0] != '\0' && strlen(command) < sizeof(at_command->command)) {
    at_command = (at_serial_port_command_t *)malloc(sizeof(struct at_serial_port_command_s));
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

static void at_serial_port_wait_command_response(serial_port_t *port,
                                                 at_serial_port_command_t *at_command)
{
  char *sob = at_command->result;
  char *current = sob;
  char *eob = sob + sizeof(at_command->result);
  int num_read = 0;

  /* read characters into our string buffer until we get a CR or NL */
  while ((num_read = (int)read(port->fd, current, (size_t)(eob - current) - 1)) > 0) {
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

int at_serial_port_execute_command(serial_port_t *port, at_serial_port_command_t *at_command)
{
  int ret = 0;
  serial_port_open(port);
  if (!serial_port_is_open(port)) {
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
    piksi_log(LOG_ERR,
              "Unable to resolve result of command '%s' to port '%s'. %d characters returned",
              at_command->command,
              port->port_name,
              at_command->result_length);
    ret = -1;
    goto cleanup;
  }

cleanup:
  serial_port_close(port);
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
  -113,                                                   // zero case
  -111, -109, -107, -105, -103, -101, -99, -97, -95, -93, // 1 - 30 cases
  -91,  -89,  -87,  -85,  -83,  -81,  -79, -77, -75, -73,
  -71,  -69,  -67,  -65,  -63,  -61,  -59, -57, -55, -53,
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
  0.2f,
  0.3f,
  0.6f,
  1.2f,
  2.4f,
  4.8f,
  9.6f,
  12.8f,
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
int at_command_report_signal_quality(serial_port_t *port, s8 *signal_strength, float *error_rate)
{
  int ret = 0;
  const char *at_prefix = AT_COMMAND_PREFIX;
  const char *base_command = AT_COMMAND_CSQ;
  char command_buffer[AT_COMMAND_BUFFER_STACK_MAX];
  snprintf(command_buffer, sizeof(command_buffer), "%s%s", at_prefix, base_command);

  at_serial_port_command_t *at_command = at_serial_port_command_create(command_buffer);
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
