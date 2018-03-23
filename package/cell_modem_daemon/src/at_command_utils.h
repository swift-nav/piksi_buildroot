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

/**
 * @brief Serial Port Context
 */
typedef struct at_serial_port_s at_serial_port_t;

/**
 * @brief at_serial_port_create - allocate a new serial port context
 * @param port_name: file descriptor for the serial port
 * @return newly allocated serial port context, or NULL
 */
at_serial_port_t *at_serial_port_create(const char *port_name);

/**
 * @brief at_serial_port_destroy - deallocate serial port context
 * @param port_loc: address of the serial port context pointer
 */
void at_serial_port_destroy(at_serial_port_t **port_loc);

/**
 * @brief at_serial_port_open - opens the serial port for IO
 * @param port: serial port context
 */
void at_serial_port_open(at_serial_port_t *port);

/**
 * @brief at_serial_port_close - release a previously opened serial port
 * @param port: serial port context
 */
void at_serial_port_close(at_serial_port_t *port);

/**
 * @brief at_serial_port_is_open - check if port is curretly open
 * @param port: serial port context
 * @return True if port is open, False if not
 */
bool at_serial_port_is_open(at_serial_port_t *port);

/**
 * @brief AT Command Context
 */
typedef struct at_serial_port_command_s at_serial_port_command_t;

/**
 * @brief at_serial_port_command_create - allocate a new at command context
 * @param command: string with fully specified command to execute (without return)
 * @return newly allocated at command context, or NULL
 */
at_serial_port_command_t *at_serial_port_command_create(const char *command);

/**
 * @brief at_serial_port_command_destroy - deallocate an at command context
 * @param at_command_loc: address of the at command pointer
 */
void at_serial_port_command_destroy(at_serial_port_command_t **at_command_loc);

/**
 * @brief at_serial_port_execute_command - execure the command on a specified port
 * @param port: serial port context to which the command will be applied
 * @param at_command: at command context to apply
 * @return 0 if nominal, -1 if a failure occured
 */
int at_serial_port_execute_command(at_serial_port_t *port,
                                   at_serial_port_command_t *at_command);

/**
 * @brief at_serial_port_command_result - retrieve the full command response
 * @param at_command: at command context
 * @return const pointer to the response string
 */
const char *at_serial_port_command_result(at_serial_port_command_t *at_command);

/**
 * @brief at_command_report_signal_quality - perform AT+CSQ
 * @param port: serial port context
 * @param signal_strength: signal strength in dBm
 * @param error_rate: BER error rate as a percent
 * @return 0 if nominal, -1 if a failure occured
 */
int at_command_report_signal_quality(at_serial_port_t *port,
                                     s8 *signal_strength,
                                     float *error_rate);

#endif /* SWIFTNAV_AT_COMMAND_UTILS_H */
