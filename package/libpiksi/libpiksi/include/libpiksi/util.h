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
 * @file    util.h
 * @brief   Utilities API.
 *
 * @defgroup    util Util
 * @addtogroup  util
 * @{
 */

#ifndef LIBPIKSI_UTIL_H
#define LIBPIKSI_UTIL_H

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <libpiksi/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Call snprintf and assert if the result is truncated
 * @details snprintf() and do not write more than size bytes (including the
 *          terminating null byte ('\0')). If the output was truncated due to
 *          this limit then the return value is the number of characters
 *          (excluding the terminating null byte) which would have been written
 *          to the final string if enough space had been available. Thus,
 *          a return value of size or more means that the output was truncated.
 *
 * @return  void
 */
void snprintf_assert(char *s, size_t n, const char *format, ...);

/**
 * @brief   Call snprintf and log a warning if the result is truncated
 * @details snprintf_assert() documentation for details
 *
 * @return  True if success (ie. no truncation)
 */
bool snprintf_warn(char *s, size_t n, const char *format, ...);

/**
 * @brief   Get the SBP sender ID for the system.
 * @details Returns the board-specific SBP sender ID.
 *
 * @return                  The SBP sender ID.
 */
u16 sbp_sender_id_get(void);

/**
 * @brief   Get system uptime
 * @return  Uptime in milliseconds
 */
u64 system_uptime_ms_get(void);

/**
 * @brief   Get the Device UUID for the system.
 * @details Returns the board-specific UUID.
 *
 * @return                  The operation result.
 * @retval 0                The value was returned successfully.
 * @retval -1               An error occurred.
 */
int device_uuid_get(char *str, size_t str_size);

/**
 * @brief   Determine if the current system is Duro
 * @details Returns the true or false
 *
 * @return  True if the current system is a Duro
 */
bool device_is_duro(void);

/**
 * @brief   Get the hardware version string.
 * @details Represents the major and minor version for the device.
 *
 * @return                  The operation result.
 * @retval 0                The value was returned successfully.
 * @retval -1               An error occurred.
 */
int hw_version_string_get(char *hw_version_string, size_t size);

/**
 * @brief   Get the hardware revision string.
 * @details Represents the name of the device hardware revision.
 *
 * @return                  The operation result.
 * @retval 0                The value was returned successfully.
 * @retval -1               An error occurred.
 */
int hw_revision_string_get(char *hw_revision_string, size_t size);

/**
 * @brief   Get the hardware variant string.
 * @details Represents the variant of device for the given hardware revision.
 *
 * @return                  The operation result.
 * @retval 0                The value was returned successfully.
 * @retval -1               An error occurred.
 */
int hw_variant_string_get(char *hw_variant_string, size_t size);

/**
 * @brief   Get the product id.
 * @details Customer facing product name.
 *
 * @return                  The operation result.
 * @retval 0                The value was returned successfully.
 * @retval -1               An error occurred.
 */
int product_id_string_get(char *product_id_string, size_t size);

/**
 * @brief   Determine if device has GPS time
 * @details Returns true or false
 *
 * @return  True if system has GPS time, false otherwise
 */
bool device_has_gps_time(void);

/**
 * @brief   Set resource indicating if device has GPS time
 * @details Write variable to /var/run/health/gps_time_available
 *
 */
void set_device_has_gps_time(bool has_time);

/**
 * @brief   Read a string from file
 * @details Reads characters from a file and stores them as a C string into str
 *          until (str_size-1) characters have been read or either a newline or
 *          the end-of-file is reached, whichever happens first. Str shall be
 *          null terminated. If buffer is too small and the string is truncated,
 *          a warning shall be issued but result considered as a success.
 *
 *          Buffer shall be zero initialized before reading the string.
 *
 * @return  0 when success
 */
int file_read_string(const char *filename, char *str, size_t str_size);

/**
 * @brief   Read status from file
 * @details File system holds multiple files indicating if some feature/state
 *          is available. This function offers simple way to fetch this status.
 *
 * @return  True if status value is '1', false otherwise
 */
bool file_read_value(char *file_path);


/**
 * @brief   Write string to file
 * @details This function writes string to the specified file name
 *
 * @return  Returns false if failed, true otherwise
 */
bool file_write_string(const char *filename, const char *str);

/**
 * @brief   Append string to file
 * @details This function appends string to the end of specified file name
 *
 * @return  Returns false if failed, true otherwise
 */
bool file_append_string(const char *filename, const char *str);

typedef void (*child_exit_fn_t)(pid_t pid);

void reap_children(bool debug, child_exit_fn_t exit_handler);

void setup_sigchld_handler(void (*handler)(int));
void setup_sigint_handler(void (*handler)(int signum, siginfo_t *info, void *ucontext));
void setup_sigterm_handler(void (*handler)(int signum, siginfo_t *info, void *ucontext));

typedef struct sigwait_params_s {
  sigset_t waitset;
  siginfo_t info;
  struct timespec timeout;
} sigwait_params_t;

/**
 * @brief   Wrapper for sigtimedwait setup actions
 * @details Initializes the signal set and timeout struct
 *
 * @return  0 if success
 */
int setup_sigtimedwait(sigwait_params_t *params, int sig, time_t tv_sec);

/**
 * @brief   Update the timeout value
 *
 * @return  0 if success
 */
int update_sigtimedwait(sigwait_params_t *params, time_t tv_sec);

/**
 * @brief   Wrapper for sigtimedwait
 * @details Sleep until timeout is reached or signal received
 *
 * @return  0 if signal was received, 1 if timeout was reached
 */
int run_sigtimedwait(sigwait_params_t *params);

/**
 * @brief   Check if file descriptor is file
 * @details This function is useful for avoiding operations that fail on
 *          non-file FDs, for example, lseek will fail with ESPIPE if the FD
 *          is associated with a pipe, socket or FIFO.
 *
 * @return  True if file, false otherwise
 */
bool is_file(int fd);

/**
 * @brief   Start a child process and record its output
 * @details Forks a child process and redirect its stdin from the input file
 *          and its stdout to provided output buffer. Call blocks until the
 *          output buffer is full or child process stdout gives EOF.
 *
 * @param[in]    input_file   Input file to map as child process stdin
 * @param[in]    cmd          Command to run within child process
 * @param[in]    cmd          Command arguments, list shall be NULL terminated,
 *                            see execvp manual
 * @param[inout] output       Output buffer
 * @param[in]    output_size  Output buffer size
 *
 * @return                    The operation result.
 * @retval 0                  Success
 */
int run_with_stdin_file(const char *input_file,
                        const char *cmd,
                        char *const argv[],
                        char *output,
                        size_t output_size);

/**
 * @brief   Check if string contains only digits
 * @details Empty string ("\0") and NULL are considered non-digits-only.
 *          A string with leading zeros is considered as a valid digits-only.
 *
 * @return  True if only digits in str
 */
bool str_digits_only(const char *str);

#define SWFT_MAX(a, b)  \
  ({                    \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a > _b ? _a : _b;  \
  })

#define SWFT_MIN(a, b)  \
  ({                    \
    typeof(a) _a = (a); \
    typeof(b) _b = (b); \
    _a < _b ? _a : _b;  \
  })

#define COUNT_OF(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

#ifdef __cplusplus
}
#endif

#endif /* LIBPIKSI_UTIL_H */

/** @} */
