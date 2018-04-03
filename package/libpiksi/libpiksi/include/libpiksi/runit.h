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

/**
 * @brief   Start a dynamic runit service
 * @details Builds a runit service directory with the supplied information
 *          and starts the service.
 *
 * @param[in] service_dir   The top-level runit dir where dynamic services will
 *                          be located.
 * @param[in] service_name  The name of the dynamic service
 * @param[in] command_line  The command line to run for the service
 * @param[in] restart       True if this service should be relaunched if it
 *                          stops.
 *
 * @return                  The operation result.
 * @retval 0                The value was returned successfully.
 * @retval -1               An error occurred.
 */
int start_runit_service(const char* service_dir,
                        const char* service_name,
                        const char* command_line,
                        bool restart);

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
int stop_runit_service(const char* service_dir, const char* service_name);

#endif /* LIBPIKSI_RUNIT_H */

/** @} */
