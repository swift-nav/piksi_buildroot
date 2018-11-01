/*
 * Copyright (C) 2017-2018 Swift Navigation Inc.
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
 * @defgroup    util Util
 * @addtogroup  util
 * @{
 */

#ifndef LIBPIKSI_UTIL_H
#define LIBPIKSI_UTIL_H

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <libpiksi/common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The NESTED_FN_TYPEDEF creates a typedef sufficient to represent a nested
 * function on clang and gcc.  On clang, we must use the "blocks" feature,
 * for gcc we can used nested functions, which can be captured by a normal
 * function pointer.
 *
 * Example:
 *
 * ```
 * NESTED_FN_TYPEDEF(bool, line_fn_t, const char *line);
 * ```
 */
#ifdef __clang__
#define NESTED_FN_TYPEDEF(RetTy, Name, ...) typedef RetTy (^Name)(__VA_ARGS__);
#else
#define NESTED_FN_TYPEDEF(RetTy, Name, ...) typedef RetTy (*Name)(__VA_ARGS__);
#endif

/**
 * On clang, a nested function (block) must annotate data outside its scope
 *   that it wishes to modify.
 *
 * Example:
 *
 * ```
 * int NESTED_AXX(line_count) = 0;
 * ```
 */
#ifdef __clang__
#define NESTED_AXX(F) __block F
#else
#define NESTED_AXX(F) F
#endif

/**
 * Wrapper to define a nested function (block on clang) suitable for clange
 * or for gcc.
 *
 * Example:
 *
 * ```
 * NESTED_FN(bool, (const char *line),
 * {
 *   state->pid_count++;
 *   return parse_ps_cpu_mem_line(line, state);
 * })
 * ```
 */
#ifdef __clang__
#define NESTED_FN(RetTy, ArgSpec, FnBody) ^RetTy ArgSpec FnBody
#else
#define NESTED_FN(RetTy, ArgSpec, FnBody) \
  ({                                      \
    RetTy __fn__ ArgSpec FnBody;          \
    __fn__;                               \
  })
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

NESTED_FN_TYPEDEF(ssize_t, buffer_fn_t, char *buffer, size_t buflen, void *context);

/** @brief Configures the run_command function */
typedef struct {
  const char *input;       /**< The input file for the process */
  const char *const *argv; /**< The argv of the command, e.g. {"ls", "foo", "bar", "baz"} */
  char *buffer;            /**< The buffer to write output data to */
  size_t length;           /**< The length of the output buffer */
  buffer_fn_t func;        /**< A function to receive output data */
  void *context;           /**< A context to pass to the function */
} run_command_t;

/**
 * @brief   Start a child process and record its output
 * @details Forks a child process and redirect its stdout to provided output buffer.
 *          Call blocks until the output buffer is full or child process stdout
 *          gives EOF.
 *
 * @param[in]    r Command config to run within child process
 *
 * @return                    The operation result.
 * @retval 0                  Success
 */
int run_command(const run_command_t *r);

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
                        const char *const argv[],
                        char *output,
                        size_t output_size);

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
 * @param[in]    buffer_fn    Function which will process output data
 * @param[in]    context      A context to pass to buffer_fn
 *
 * @return                    The operation result.
 * @retval 0                  Success
 */
int run_with_stdin_file2(const char *input_file,
                         const char *cmd,
                         const char *const argv[],
                         char *output,
                         size_t output_size,
                         buffer_fn_t buffer_fn,
                         void *context);

/**
 * @brief   Check if string contains only digits
 * @details Empty string ("\0") and NULL are considered non-digits-only.
 *          A string with leading zeros is considered as a valid digits-only.
 *
 * @return  True if only digits in str
 */
bool str_digits_only(const char *str);

bool strtoul_all(int base, const char *str, unsigned long *value);

bool strtod_all(const char *str, double *value);

typedef struct pipeline_s pipeline_t;

/**
 * A utility class for running pipelines, similar to a shell pipeline.
 *
 * For example:
 *
 * ```
 * pipeline_t *SCRUB(r, s_pipeline) = create_pipeline();
 * r = r->cat(r, EXTRACE_LOG);
 * r = r->pipe(r);
 * r = r->call(r, "grep", (const char *const[]){"grep", "-c", "^[0-9][0-9]*[+] ", NULL});
 *
 * r = r->wait(r);
 *
 * if (r->is_nil(r)) { handle_failure(...); }
 * else ...
 * ```
 */
typedef struct pipeline_s {

  /**
   * Stage a file as input to the next component of the pipeline.
   */
  pipeline_t *(*cat)(pipeline_t *r, const char *filename);

  /**
   * Takes anything previously staged as input (from @c cat or @c call) and
   *   opens up corresponding filesystem objects to pipe output to the next
   *   component of the pipeline.
   */
  pipeline_t *(*pipe)(pipeline_t *r);

  /**
   * Stages a subprocess exec into the pipeline.  Arguments are similar
   * to that of execv and friends.
   */
  pipeline_t *(*call)(pipeline_t *r, const char *prog, const char *const argv[]);

  /**
   * Wait for the pipeline to complete (and capture it's output).
   */
  pipeline_t *(*wait)(pipeline_t *r);

  /**
   * Ask if the pipeline is "nil", a nil pipeline indicates that a failure
   *   occured while executing.
   */
  bool (*is_nil)(pipeline_t *r);

  /**
   * Clean-up resources used by this pipeline object.
   */
  pipeline_t *(*destroy)(pipeline_t *r);

  /** The stdout of the command that was run, output is truncated at 4k */
  char stdout_buffer[4096];

  /** The exit code of the pipeline, usually the exit code of the last process, or
   *  and artificial exit code if there was usage error.
   */
  int exit_code;

  const char *_filename;
  int _fd_stdin;

  const char *_proc_path;
  const char *const *_proc_args;

  bool _is_nil;

  int _closeable_fds[16];
  size_t _closeable_idx;

} pipeline_t;

pipeline_t *create_pipeline(void);

/**
 * Attached a clean-up function for a variable.
 *
 * The function that's attached to the variable will be called with a pointer
 * to that variable when the variable goes out of scope.
 *
 * Example:
 * ```
 * pipeline_t *SCRUB(r, s_pipeline) = create_pipeline();
 * ```
 *
 * With `s_pipeline` defined as follows:
 * ```
 * static void s_pipeline(pipeline_t **r)
 * {
 *   if (r == NULL || *r == NULL) return;
 *   *r = (*r)->destroy(*r);
 * };
 * ```
 */
#define SCRUB(TheVar, TheFunc) (TheVar) __attribute__((__cleanup__(TheFunc)))

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
