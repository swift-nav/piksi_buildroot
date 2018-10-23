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

typedef enum {
  FT_F64,
  FT_U16,
  FT_U32,
  FT_STR,
} field_type_t;

typedef struct {
  union {
    u16 *u16;
    u32 *u32;
    double *f64;
    char *str;
  } dst;
  field_type_t type;
  int next;
  const char *desc;
  size_t buflen;
} line_spec_t;

bool parse_ps_line(const char *line, int start_state, int final_state, line_spec_t *line_specs);
bool parse_tab_line(const char *line,
                    int start_state,
                    int final_state,
                    line_spec_t *line_specs,
                    const char *separators);

int count_lines(const char *file_path);
int count_sz_lines(const char *sz);

unsigned long fetch_mem_total(void);

NESTED_FN_TYPEDEF(bool, line_fn_t, const char *line);

typedef struct {
  char *buf;
  size_t size;
  size_t line_count;
} leftover_t;

/**
 * @brief Iterate over every line in a buffer.
 *
 * @param lines[in]          the buffer that contains line delimited data
 * @param leftover[out]      a buffer that receives any partial lines at the end of the buffer
 * @param leftover_size[out] the amount of data in the leftover
 * @param line_fn[in]        the function that will be called for each line
 *
 * @return The number of characters that were consumed from the buffer, not including the
 *         null terminator of the string (could include "leftover" bytes from a previous
 *         invocation) ... or, -1, if their was leftover, and no leftover structure was
 *         provided.
 */
ssize_t foreach_line(const char *lines, leftover_t *leftover, line_fn_t line_fn);

#ifdef __cplusplus
}
#endif

#endif /* RESMON_COMMON_H */

/** @} */
