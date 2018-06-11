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
 * Add to piksi_log to send the log message to SBP as well as
 *   the system log (syslog).
 *
 *  E.g. `piksi_log(LOG_SBP|LOG_ERROR, "Some message");`
 */
#define LOG_SBP LOG_LOCAL1

#ifdef __cplusplus
extern "C" {
#endif

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
 * @brief   Log to stdout only - for host testing purposes only.
 *
 * @param[in] enable        Enable or disable logging to stdout only
 */
void logging_log_to_stdout_only(bool enable);

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

/**
 * @brief   Send a log message over SBP
 *
 * @param[in] priority      Priority level as defined in <syslog.h>.
 * @param[in] msg_text      The log message text to send.
 */
void sbp_log(int priority, const char *msg_text, ...);

/**
  * @brief   Send a log message over SBP
  *
  * @param[in] priority      Priority level as defined in <syslog.h>.
  * @param[in] msg_text      The log message text to send.
  * @param[in] ap            variable argument list for printf().
  */
void sbp_vlog(int priority, const char *msg_text, va_list ap);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBPIKSI_LOGGING_H */

/** @} */
