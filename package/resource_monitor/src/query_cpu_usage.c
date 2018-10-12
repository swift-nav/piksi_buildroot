/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include <libsbp/linux.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "resource_query.h"

#define ITEM_COUNT 10

#define THREAD_NAME_MAX 32
#define COMMAND_LINE_MAX 256


typedef struct {
  u16 pid;
  u8 pcpu;
  char thread_name[THREAD_NAME_MAX];
  char command_line[COMMAND_LINE_MAX];
} query_item_t;

static struct { query_item_t items[ITEM_COUNT]; } query_context;

static void *init_cpu_query() { return NULL; }

enum {
  STATE_PID,
  STATE_PCPU,
  STATE_THREAD_NAME,
  STATE_COMMAND_LINE,
  STATE_COMMAND_DONE,
};

static bool parse_ps_line(const char *line, const size_t item_index) {

  int state = STATE_PID;
  char *tab_ctx = NULL;

  fprintf(stderr, "line: '%s'\n", line);
  char *line_a = strdupa(line);

  for (char *field = strtok_r(line_a, "\t", &tab_ctx); field != NULL;
       field = strtok_r(NULL, "\t", &tab_ctx)) {

    fprintf(stderr, "field: %s\n", field);

    switch (state) {
    case STATE_PID: {
      unsigned long pid = 0;
      if (!strtoul_all(10, field, &pid)) {
        piksi_log(LOG_ERR, "%s: failed to parse pid value: %s", __FUNCTION__,
                  field);
        return false;
      }
      query_context.items[item_index].pid = (u16)pid;
      state = STATE_PCPU;
    } break;

    case STATE_PCPU: {
      double pcpu_double = 0;
      if (!strtod_all(field, &pcpu_double)) {
        piksi_log(LOG_ERR, "%s: failed to parse pcpu value: %s", __FUNCTION__,
                  field);
        return false;
      }
      query_context.items[item_index].pcpu =
          (u8)((1u << (sizeof(u8) * 8)) * (pcpu_double / 100.0));
      state = STATE_THREAD_NAME;
    } break;

    case STATE_THREAD_NAME: {
      strncpy(query_context.items[item_index].thread_name, field,
              sizeof(query_context.items[0].thread_name));
      state = STATE_COMMAND_LINE;
    } break;

    case STATE_COMMAND_LINE: {
      strncpy(query_context.items[item_index].command_line, field,
              sizeof(query_context.items[0].command_line));
      state = STATE_COMMAND_DONE;
    } break;

    case STATE_COMMAND_DONE:
    default:
      piksi_log(LOG_ERR, "%s: found too many fields: %s", __FUNCTION__,
                field);
      return false;
    }
  }

  if (state != STATE_COMMAND_DONE) {
    piksi_log(LOG_ERR, "%s: did not find enough fields", __FUNCTION__);
    return false;
  }

  return true;
}

static void run_cpu_query(void *context) {

  (void)context;

  char *argv[] = {"ps", "--no-headers", "-e",
                  "-o", "%p\t%C\t%c\t%a", "--sort=-pcpu",
                  NULL};

  char buf[4096] = {0};
  int rc = run_with_stdin_file(NULL, "ps", argv, buf, sizeof(buf));

  if (rc != 0) {
    piksi_log(LOG_ERR | LOG_SBP, "error running 'ps' command: %s",
              strerror(errno));
  }

  size_t item_index = 0;
  char *line_ctx = NULL;

  memset(&query_context, 0, sizeof(query_context));

  for (char *line = strtok_r(buf, "\n", &line_ctx); line != NULL;
       line = strtok_r(NULL, "\n", &line_ctx)) {

    if (item_index >= ITEM_COUNT) {
      break;
    }

    if (!parse_ps_line(line, item_index++)) {
      return;
    }
  }

  for (size_t i = 0; i < ITEM_COUNT; i++) {
    fprintf(stderr, "%zu: %d %d %s %s\n", i, query_context.items[i].pid,
            query_context.items[i].pcpu, query_context.items[i].thread_name,
            query_context.items[i].command_line);
  }
}

static void prepare_cpu_query_sbp(u8 *sbp_buf, void *context) {
  (void)context;
  (void)sbp_buf;
}

static void teardown_cpu_query(void *context) { (void)context; }

resq_interface_t query_cpu = {
    .init = init_cpu_query,
    .run_query = run_cpu_query,
    .prepare_sbp = prepare_cpu_query_sbp,
    .teardown = teardown_cpu_query,
};

static __attribute__((constructor)) void register_cpu_query() {
  resq_register(&query_cpu);
}

