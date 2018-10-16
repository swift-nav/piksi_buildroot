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
 * @file    parse_ps_line.h
 * @brief   Utility for parsing output from ps line
 *
 * @defgroup    parse_ps_line
 * @addtogroup  parse_ps_line
 * @{
 */

#ifndef RESMON_COMMON_H
#define RESMON_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libpiksi/common.h>

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

int count_lines(const char *file_path);

unsigned long fetch_mem_total(void);

#ifdef __cplusplus
}
#endif

#endif /* RESMON_COMMON_H */

/** @} */
