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

/**
 * @file    util.h
 * @brief   Utilities API.
 *
 * @defgroup    runit Runit service utilities
 * @addtogroup  runit
 * @{
 */

#ifndef LIBPIKSI_RUNIT_H
#define LIBPIKSI_RUNIT_H

#include <libpiksi/common.h>

typedef struct {
  /** The top-level runit dir where dynamic services will be located. */
  const char* service_dir;
  /** The name of the dynamic service */
  const char* service_name;
  /** The command line to run for the service */
  const char* command_line;
  /** A custom "down" command, if any, this is run when the service receives a request to stop. */
  const char* custom_down;
  /** A custom "finish" command, if any, this is run after the service stops. */
  const char* finish_command;
  /** True if this service should be relaunched if it stops. */
  bool restart;
} runit_config_t;

/**
 * @brief   Start a dynamic runit service
 * @details Builds a runit service directory with the supplied information
 *          and starts the service.
 *
 * @param[in] cfg  The runit configuration, see @c runit_config_t
 *
 * @return                  The operation result.
 * @retval 0                The value was returned successfully.
 * @retval -1               An error occurred.
 */
int start_runit_service(runit_config_t *cfg);

typedef enum {
  /** Status is unknown, or an error occurred */
  RUNIT_UNKNOWN = -1,
  /** No control file was found in order to determine the status */
  RUNIT_NO_STAT,
  /** No pid file was found */
  RUNIT_NO_PID,
  /** The service is running */
  RUNIT_RUNNING,
  /** The service is running, but also has some other status */
  RUNIT_RUNNING_OTHER,
  /** The service is down */
  RUNIT_DOWN,
} runit_stat_t;

/**
 * @brief          Checks the status of a runit service
 * @param[in] cfg  The runit configuration, see @c runit_config_t
 * @return         The status of the runit service, see @c runit_stat_t
 */
runit_stat_t stat_runit_service(runit_config_t *cfg);

/** @brief Convert @c runit_stat_t to human readable string.
 */
const char* runit_status_str(runit_stat_t status);

/**
 * @brief   Stop a runit service
 * @details Stops a runit service located in the specified directory by writing
 *          to the services control socket.
 *
 * @param[in] service_dir   The top-level runit dir where services are
 *                          be located.
 * @param[in] service_name  The name of the service to stop
 *                          stops.
 *
 * @return                  The operation result.
 * @retval 0                The value was returned successfully.
 * @retval -1               An error occurred.
 */
int stop_runit_service(runit_config_t *cfg);

#endif /* LIBPIKSI_RUNIT_H */

/** @} */
