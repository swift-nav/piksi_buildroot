/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file    logging.h
 * @brief   Logging API.
 *
 * @defgroup    logging Logging
 * @addtogroup  logging
 * @{
 */

#ifndef LIBPIKSI_LOGGING_H
#define LIBPIKSI_LOGGING_H

#include <libpiksi/common.h>
#include <syslog.h>

/**
 * @brief   Initialize logging.
 * @details Initialize the global logging state for the process.
 * @note    This function should be called before using other API functions
 *          so that logging is properly initialized.
 *
 * @param[in] identity      String identifying the process.
 */
int logging_init(const char *identity);

/**
 * @brief   Deinitialize logging.
 * @details Deinitialize the global logging state for the process.
 */
void logging_deinit(void);

/**
 * @brief   Log a message.
 * @details Write a message to the system log.
 *
 * @param[in] priority      Priority level as defined in <syslog.h>.
 * @param[in] format        Format string and arguments as defined by printf().
 */
void piksi_log(int priority, const char *format, ...);

/**
 * @brief   Log a message with variable argument list.
 * @details Write a message to the system log using a variable argument list.
 *
 * @param[in] priority      Priority level as defined in <syslog.h>.
 * @param[in] format        Format string as defined by printf().
 * @param[in] ap            Variable argument list for printf().
 */
void piksi_vlog(int priority, const char *format, va_list ap);

#endif /* LIBPIKSI_LOGGING_H */

/** @} */
