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

#ifndef SWIFTNAV_AT_COMMAND_UTILS_H
#define SWIFTNAV_AT_COMMAND_UTILS_H


typedef struct at_serial_port_s at_serial_port_t;

at_serial_port_t *at_serial_port_create(const char *port_name);
void at_serial_port_destroy(at_serial_port_t **port_loc);
void at_serial_port_open(at_serial_port_t *port);
void at_serial_port_close(at_serial_port_t *port);
bool at_serial_port_is_open(at_serial_port_t *port);

typedef struct at_serial_port_command_s at_serial_port_command_t;

at_serial_port_command_t *at_serial_port_command_create(const char *command);
void at_serial_port_command_destroy(at_serial_port_command_t **at_command_loc);
void at_serial_port_execute_command(at_serial_port_t *port,
                                    at_serial_port_command_t *at_command);
const char *at_serial_port_command_result(at_serial_port_command_t *at_command);

/* AT+CSQ */
int at_command_report_signal_quality(at_serial_port_t *port,
                                     s8 *signal_strength,
                                     float *error_rate);

/* AT+CPIN? */
/* AT+CCID */
/* AT+CREG? */
/* AT+CGREG? */
/* AT+COPS? */

#endif /* SWIFTNAV_AT_COMMAND_UTILS_H */
