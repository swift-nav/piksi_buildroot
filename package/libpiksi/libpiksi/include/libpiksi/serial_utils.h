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

#ifndef SWIFTNAV_SERIAL_COMMAND_UTILS_H
#define SWIFTNAV_SERIAL_COMMAND_UTILS_H
#define SERIAL_PORT_NAME_MAX_LENGTH (128u)
#include <termios.h>
/**
 * @brief Serial Port Context
 */
typedef struct serial_port_s {
  char port_name[SERIAL_PORT_NAME_MAX_LENGTH];
  int fd;
  bool is_open;
} serial_port_t;

/**
 * @brief serial_port_create - allocate a new serial port context
 * @param port_name: file descriptor for the serial port
 * @return newly allocated serial port context, or NULL
 */

serial_port_t *serial_port_create(const char *port_name);

/**
 * @brief serial_port_destroy - deallocate serial port context
 * @param port_loc: address of the serial port context pointer
 */
void serial_port_destroy(serial_port_t **port_loc);

/**
 * @brief serial_port_open - opens the serial port for IO
 * @param port: serial port context
 */
void serial_port_open(serial_port_t *port);

/**
 * @brief serial_port_open - opens the serial port for IO
 * @param port: serial port context
 */
void serial_port_open_baud(serial_port_t *port, speed_t baudrate);

/**
 * @brief serial_port_close - release a previously opened serial port
 * @param port: serial port context
 */
void serial_port_close(serial_port_t *port);

/**
 * @brief serial_port_is_open - check if port is curretly open
 * @param port: serial port context
 * @return True if port is open, False if not
 */
bool serial_port_is_open(serial_port_t *port);

#endif /* SWIFTNAV_SERIAL_COMMAND_UTILS_H */
