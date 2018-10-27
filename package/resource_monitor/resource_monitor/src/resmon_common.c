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

#define _GNU_SOURCE

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "resmon_common.h"

#define PROC_MEMINFO "/proc/meminfo"

bool parse_ps_line(const char *line, int start_state, int final_state, line_spec_t *line_specs)
{
  return parse_tab_line(line, start_state, final_state, line_specs, "\t");
}

bool parse_tab_line(const char *line,
                    int start_state,
                    int final_state,
                    line_spec_t *line_specs,
                    const char *separators)
{
  int state = start_state;
  char *tab_ctx = NULL;
  char *line_a = strdupa(line);

  for (char *field = strtok_r(line_a, separators, &tab_ctx); field != NULL;
       field = strtok_r(NULL, separators, &tab_ctx)) {

    if (state == final_state) {
      piksi_log(LOG_WARNING,
                "%s: found too many fields (in state: %s): %s",
                __FUNCTION__,
                line_specs[state].desc,
                field);
      return false;
    }

    switch (line_specs[state].type) {
    case FT_U16: {
      unsigned long ul_value = 0;
      if (!strtoul_all(10, field, &ul_value)) {
        piksi_log(LOG_WARNING,
                  "%s: failed to parse %s value: %s",
                  __FUNCTION__,
                  line_specs[state].desc,
                  field);
        return false;
      }
      *line_specs[state].dst.u16 = (u16)ul_value;
      state = line_specs[state].next;
    } break;

    case FT_U32: {
      unsigned long ul_value = 0;
      if (!strtoul_all(10, field, &ul_value)) {
        piksi_log(LOG_WARNING,
                  "%s: failed to parse %s value: %s",
                  __FUNCTION__,
                  line_specs[state].desc,
                  field);
        return false;
      }
      *line_specs[state].dst.u32 = (u32)ul_value;
      state = line_specs[state].next;
    } break;

    case FT_F64: {
      double f64_value = 0;
      if (!strtod_all(field, &f64_value)) {
        piksi_log(LOG_WARNING,
                  "%s: failed to parse %s value: %s",
                  __FUNCTION__,
                  line_specs[state].desc,
                  field);
        return false;
      }
      *line_specs[state].dst.f64 = f64_value;
      state = line_specs[state].next;
    } break;

    case FT_STR: {
      strncpy(line_specs[state].dst.str, field, line_specs[state].buflen);
      state = line_specs[state].next;
    } break;

    default: piksi_log(LOG_WARNING, "%s: invalid field type", __FUNCTION__); return false;
    }
  }

  if (state != final_state) {
    piksi_log(LOG_WARNING,
              "%s: did not find enough fields (last state: %s)",
              __FUNCTION__,
              line_specs[state].desc);
    return false;
  }

  return true;
}

int count_sz_lines(const char *sz)
{
  assert(sz != NULL);

  if (*sz == '\0') return 0;

  int count = 0;
  bool newline_eof = false;

  while ((sz = strchr(sz, '\n')) != NULL) {
    ++count;
    ++sz;
    newline_eof = *sz == '\0';
  }

  return newline_eof ? count : count + 1;
}

int count_lines(const char *file_path)
{
  FILE *fp = fopen(file_path, "r");

  if (fp == NULL) {
    piksi_log(LOG_ERR, "%s: error opening file: %s", __FUNCTION__, strerror(errno));
    return -1;
  }

  int count = 0;

  for (;;) {
    int ret = fgetc(fp);
    if (ret == EOF) break;
    if (ret == '\n') count++;
  }

  fclose(fp);
  return count;
}

unsigned long fetch_mem_total(void)
{
  char *mem_total_sz = NULL;

  FILE *fp = fopen(PROC_MEMINFO, "r");
  if (fp == NULL) {
    PK_LOG_ANNO(LOG_ERR, "unable to open %s: %s", PROC_MEMINFO, strerror(errno));
    goto error;
  }

  int rc = fscanf(fp, "MemTotal: %ms", &mem_total_sz);

  if (rc <= 0) {
    PK_LOG_ANNO(LOG_ERR, "error reading %s: %s", PROC_MEMINFO, strerror(errno));
    goto error;
  }

  PK_LOG_ANNO(LOG_DEBUG, "read mem total: %s", mem_total_sz);

  unsigned long mem_total = 0;

  if (!strtoul_all(10, mem_total_sz, &mem_total)) {
    PK_LOG_ANNO(LOG_ERR, "invalid value: %s (errno: %s)", mem_total_sz, strerror(errno));
    goto error;
  }

  fclose(fp);
  free(mem_total_sz);

  return mem_total;

error:
  fclose(fp);
  if (mem_total_sz != NULL) {
    free(mem_total_sz);
  }

  return 0;
}

ssize_t foreach_line(const char *const lines_ro, leftover_t *const leftover, line_fn_t line_fn)
{
  size_t total_len = strlen(lines_ro);
#ifdef DEBUG_FOREACH_LINE
  PK_LOG_ANNO(LOG_DEBUG,
              "total_len: %d, leftover: %d",
              total_len,
              leftover == NULL ? 0 : leftover->size);
#endif

  char *lines = NULL;

  if (leftover != NULL && leftover->size != 0) {

    size_t sz_size = leftover->size + total_len + 1 /* NULL terminator */;

    lines = alloca(sz_size);
    lines[0] = '\0';

    strncat(lines, leftover->buf, leftover->size);
    strncat(lines, lines_ro, total_len);

    total_len += leftover->size;

  } else {
    lines = strdupa(lines_ro);
  }

  if (leftover != NULL) {
    leftover->line_count = 0;
    leftover->size = 0;
  }

  char *line_ctx = NULL;
  size_t consumed = 0;
  size_t line_count = 0;

  for (char *line = strtok_r(lines, "\n", &line_ctx); line != NULL;
       line = strtok_r(NULL, "\n", &line_ctx)) {

    size_t len = strlen(line);
    consumed += len;

    /* if we're at the end of the buffer and we're not ending exactly with a newline, we'll need to
     * come back to this data later... copy it into the leftover buffer. */
    if (consumed == total_len && lines[total_len - 1] != '\n') {
      if (leftover == NULL) {
        return -1;
      }
      bzero(leftover->buf, len + 1);
      memcpy(leftover->buf, line, len);
      leftover->size = len;
      break;
    }

    consumed += 1; /* consume the newline too */
    line_count++;

    if (!line_fn(line)) break;
  }

  if (leftover != NULL) leftover->line_count = line_count;
#ifdef DEBUG_FOREACH_LINE
  PK_LOG_ANNO(LOG_DEBUG, "consumed: %d, line_count: %d", consumed, line_count);
#endif

  return (ssize_t)consumed;
}
