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
 * @file    libskylark.h
 * @brief   Skylark API.
 *
 * @defgroup    skylark Skylark
 * @addtogroup  skylark
 * @{
 */

#ifndef SWIFTNAV_LIBSKYLARK_H
#define SWIFTNAV_LIBSKYLARK_H

#include <stdbool.h>

/**
 * @struct  skylark_config_t
 *
 * @brief   Config for skylark.
 */
typedef struct {
  const char * const url;  /**< Skylark url to connect to. */
  const char * const uuid; /**< Device UUID. */
  int fd;                  /**< File descriptor to read to or write from. */
  bool debug;              /**< Enable debugging output */
} skylark_config_t;

/**
 * @brief   Setup skylark.
 * @details Setup the global skylark state for the process.
 * @note    This function should be called BEFORE using other API functions
 *          so that skylark is properly setup.
 *
 * @return                  The operation result.
 * @retval 0                A handler returned -1.
 * @retval -1               An error occurred.
 */
int skylark_setup(void);

/**
 * @brief   Teardown skylark.
 * @details Teardown the global skylark state for the process.
 * @note    This function should be called AFTER using other API functions
 *          so that skylark is properly torn down.
 */
void skylark_teardown(void);

/**
 * @brief   Download from skylark.
 * @details Download observations and other messages from skylark.
 *
 * @param[in] config        Pointer to the config to use.
 *
 * @return                  The operation result.
 * @retval 0                A handler returned -1.
 * @retval -1               An error occurred.
 */
int skylark_download(const skylark_config_t * const config);

/**
 * @brief   Upload to skylark.
 * @details Upload observations and other messages to skylark.
 *
 * @param[in] config        Pointer to the config to use.
 *
 * @return                  The operation result.
 * @retval 0                A handler returned -1.
 * @retval -1               An error occurred.
 */
int skylark_upload(const skylark_config_t * const config);

#endif /* SWIFTNAV_LIBSKYLARK_H */
