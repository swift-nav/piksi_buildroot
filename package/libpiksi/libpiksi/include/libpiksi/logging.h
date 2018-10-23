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

#undef LOG_EMERG
#undef LOG_ALERT
#undef LOG_CRIT
#undef LOG_ERR
#undef LOG_WARNING
#undef LOG_NOTICE
#undef LOG_INFO
#undef LOG_DEBUG

#undef LOG_LOCAL0
#undef LOG_LOCAL1
#undef LOG_LOCAL2
#undef LOG_LOCAL3
#undef LOG_LOCAL4
#undef LOG_LOCAL5
#undef LOG_LOCAL6
#undef LOG_LOCAL7

#define LOG_EMERG 0u   /* system is unusable */
#define LOG_ALERT 1u   /* action must be taken immediately */
#define LOG_CRIT 2u    /* critical conditions */
#define LOG_ERR 3u     /* error conditions */
#define LOG_WARNING 4u /* warning conditions */
#define LOG_NOTICE 5u  /* normal but significant condition */
#define LOG_INFO 6u    /* informational */
#define LOG_DEBUG 7u   /* debug-level messages */

#define LOG_LOCAL0 (16u << 3u) /* reserved for local use */
#define LOG_LOCAL1 (17u << 3u) /* reserved for local use */
#define LOG_LOCAL2 (18u << 3u) /* reserved for local use */
#define LOG_LOCAL3 (19u << 3u) /* reserved for local use */
#define LOG_LOCAL4 (20u << 3u) /* reserved for local use */
#define LOG_LOCAL5 (21u << 3u) /* reserved for local use */
#define LOG_LOCAL6 (22u << 3u) /* reserved for local use */
#define LOG_LOCAL7 (23u << 3u) /* reserved for local use */

#define PK_LOG_ANNO(Pri, Msg, ...)                                                              \
  do {                                                                                          \
    piksi_log(LOG_ERR, "%s: " Msg " (%s:%d)", __FUNCTION__, ##__VA_ARGS__, __FILE__, __LINE__); \
  } while (false)

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

#define log_assert(TheExpr)                         \
  ({                                                \
    bool result = false;                            \
    if (!(TheExpr)) {                               \
      piksi_log(LOG_ERR | LOG_SBP,                  \
                "%s: assertion failed: %s (%s:%d)", \
                __FUNCTION__,                       \
                #TheExpr,                           \
                __FILE__,                           \
                __LINE__);                          \
      result = true;                                \
    };                                              \
    result;                                         \
  })

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBPIKSI_LOGGING_H */

/** @} */
