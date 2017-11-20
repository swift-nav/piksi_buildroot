/*
 * Copyright (C) 2017 Swift Navigation Inc.
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
 * @file    libnetwork.h
 * @brief   Network API.
 *
 * @defgroup    network Network
 * @addtogroup  network
 * @{
 */

#ifndef SWIFTNAV_LIBNETWORK_H
#define SWIFTNAV_LIBNETWORK_H

#include <stdbool.h>

/**
 * @struct  network_config_t
 *
 * @brief   Config for network.
 */
typedef struct {
  const char *url;  /**< Network url to connect to. */
  int fd;           /**< File descriptor to read to or write from. */
  bool debug;       /**< Enable debugging output */
} network_config_t;

/**
 * @brief   Download from ntrip.
 * @details Download observations and other messages from a CORS station.
 *
 * @param[in] config        Pointer to the config to use.
 */
void ntrip_download(const network_config_t *config);

/**
 * @brief   Download from skylark.
 * @details Download observations and other messages from skylark.
 *
 * @param[in] config        Pointer to the config to use.
 */
void skylark_download(const network_config_t *config);

/**
 * @brief   Upload to skylark.
 * @details Upload observations and other messages to skylark.
 *
 * @param[in] config        Pointer to the config to use.
 */
void skylark_upload(const network_config_t *config);

/**
 * @brief Graceful termination handler for libnetwork daemons.
 *
 * @details Grafcefully stop the upload/download loops started by
 * ntrip_download, skylark_download, or skylark_upload.
 */
void libnetwork_shutdown(void);

/**
 * @brief Cycle (reconnect) the current network connection
 */
void libnetwork_cycle_connection(void);

#endif /* SWIFTNAV_LIBNETWORK_H */
