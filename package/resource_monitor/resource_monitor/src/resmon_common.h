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

/**
 * @file    resmon_common.h
 * @brief   Utility functions for the resource_monitor
 *
 * @defgroup    resmon_common
 * @addtogroup  resmon_common
 * @{
 */

#ifndef RESMON_COMMON_H
#define RESMON_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libpiksi/common.h>
#include <libpiksi/util.h>

/**
 * Used to specify a field type for @c parse_ps_line and @c parse_tab_line
 */
typedef enum {
  FT_F64, /**< A 64-bit float value */
  FT_U16, /**< A 16-bit unsigned integer value */
  FT_U32, /**< A 32-bit unsigned integer value */
  FT_STR, /**< A string value */
} field_type_t;

/**
 * Used to specify how a field should be parsed by @c parse_ps_line or @c parse_tab_line.
 *
 * For example:
 * @code
 *     double pcpu = 0;
 *     u32 vsz = 0;
 *
 *     line_spec_t line_specs[STATE_COUNT] = {
 *       [STATE_PCPU] =
 *         (line_spec_t){
 *           .type = FT_F64,
 *           .dst.f64 = &pcpu,
 *           .desc = "pcpu",
 *           .next = STATE_VSZ,
 *         },
 *       [STATE_VSZ] =
 *         (line_spec_t){
 *           .type = FT_U32,
 *           .dst.u32 = &vsz,
 *           .desc = "vsz",
 *           .next = STATE_DONE,
 *         },
 *     };
 *
 *     bool parse_success = parse_ps_line(line, STATE_PCPU, STATE_DONE, line_specs);
 *     if (!parse_success) return false;
 * @endcode
 */
typedef struct {
  field_type_t type;
  int next;
  const char *desc;
  size_t buflen;
  union {
    u16 *u16;
    u32 *u32;
    double *f64;
    char *str;
  } dst;
} line_spec_t;

/**
 * @brief Parse a 'ps' style line
 *
 * @param line[in]        the line of output to parse
 * @param start_state[in] the starting state of the line parse
 * @param final_state[in] the ending state of the line parse, it's an error
 *                        to not end on this state
 * @param line_specs[in]  the line specifications, see @c line_spec_t
 *
 * @returns true if the parse succeeds
 */
bool parse_ps_line(const char *line, int start_state, int final_state, line_spec_t *line_specs);

/**
 * @brief Parse a 'ps' style line
 *
 * @param line[in]        the line of output to parse
 * @param start_state[in] the starting state of the line parse
 * @param final_state[in] the ending state of the line parse, it's an error
 *                        to not end on this state
 * @param line_specs[in]  the line specifications, see @c line_spec_t
 *
 * @returns true if the parse succeeds
 */
bool parse_tab_line(const char *line,
                    int start_state,
                    int final_state,
                    line_spec_t *line_specs,
                    const char *separators);

/**
 * @brief Count lines in specified file.
 *
 * @param file_path[in] path to file in which to count lines
 *
 * @returns the number of lines in the file or <0 if there was an error
 */
int count_lines(const char *file_path);

/**
 * @brief The count of lines in the specified string buffer.
 *
 * @returns the count of lines in the buffer
 */
int count_sz_lines(const char *sz);

/**
 * @brief reads total system memory from /proc/meminfo
 *
 * @returns the total memory on the system, or 0 if an error occured
 */
unsigned long fetch_mem_total(void);

/**
 * @brief A function that will be called to process lines in a buffer.
 *
 * @details Processes lines in a buffer, if processing should stop, the
 * function should return false.
 */
NESTED_FN_TYPEDEF(bool, line_fn_t, const char *line);

/**
 * @brief Used to capture leftover buffer data when calling @c foreach_line
 */
typedef struct {
  char *buf;         /**< The buffer that capture leftover data, should be at least as big
                      *  as the buffer passed to @c foreach_line
                      */
  size_t size;       /**< The size of the overflowed data */
  size_t line_count; /**< The number of lines seen by @c foreach_line */
} leftover_t;

/**
 * @brief Iterate over every line in a buffer.
 *
 * @param lines[in]          the buffer that contains line delimited data
 * @param leftover[out]      a buffer that receives any partial lines at the end of the buffer
 * @param leftover_size[out] the amount of data in the leftover
 * @param line_fn[in]        the function that will be called for each line
 *
 * @returns The number of characters that were consumed from the buffer, not including the
 *          null terminator of the string (could include "leftover" bytes from a previous
 *          invocation) ... or, -1, if their was leftover, and no leftover structure was
 *          provided.
 */
ssize_t foreach_line(const char *lines, leftover_t *leftover, line_fn_t line_fn);

#ifdef __cplusplus
}
#endif

#endif /* RESMON_COMMON_H */

/** @} */
