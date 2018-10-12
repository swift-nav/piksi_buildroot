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

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <libsbp/linux.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "resource_query.h"

#define ITEM_COUNT (10u)

#define THREAD_NAME_MAX (32u)
#define COMMAND_LINE_MAX (256u)

//#define DEBUG_QUERY_CPU


typedef struct {
  u8 current_index;
} sbp_prepare_state_t;

typedef struct {
  u16 pid;
  u8 pcpu;
  char thread_name[THREAD_NAME_MAX];
  char command_line[COMMAND_LINE_MAX];
} query_item_t;

static struct {
  query_item_t items[ITEM_COUNT];
} query_context;

enum {
  STATE_PID,
  STATE_PCPU,
  STATE_THREAD_NAME,
  STATE_COMMAND_LINE,
  STATE_COMMAND_DONE,
};

static bool parse_ps_line(const char *line, const size_t item_index)
{

  int state = STATE_PID;
  char *tab_ctx = NULL;
  char *line_a = strdupa(line);

  for (char *field = strtok_r(line_a, "\t", &tab_ctx); field != NULL;
       field = strtok_r(NULL, "\t", &tab_ctx)) {

    switch (state) {
    case STATE_PID: {
      unsigned long pid = 0;
      if (!strtoul_all(10, field, &pid)) {
        piksi_log(LOG_ERR, "%s: failed to parse pid value: %s", __FUNCTION__, field);
        return false;
      }
      query_context.items[item_index].pid = (u16)pid;
      state = STATE_PCPU;
    } break;

    case STATE_PCPU: {
      double pcpu_double = 0;
      if (!strtod_all(field, &pcpu_double)) {
        piksi_log(LOG_ERR, "%s: failed to parse pcpu value: %s", __FUNCTION__, field);
        return false;
      }
      query_context.items[item_index].pcpu = (u8)((1u << (sizeof(u8) * 8)) * (pcpu_double / 100.0));
      state = STATE_THREAD_NAME;
    } break;

    case STATE_THREAD_NAME: {
      strncpy(query_context.items[item_index].thread_name,
              field,
              sizeof(query_context.items[0].thread_name));
      state = STATE_COMMAND_LINE;
    } break;

    case STATE_COMMAND_LINE: {
      strncpy(query_context.items[item_index].command_line,
              field,
              sizeof(query_context.items[0].command_line));
      state = STATE_COMMAND_DONE;
    } break;

    case STATE_COMMAND_DONE:
    default: piksi_log(LOG_ERR, "%s: found too many fields: %s", __FUNCTION__, field); return false;
    }
  }

  if (state != STATE_COMMAND_DONE) {
    piksi_log(LOG_ERR, "%s: did not find enough fields", __FUNCTION__);
    return false;
  }

  return true;
}

static void *init_resource_query()
{
  return calloc(1, sizeof(sbp_prepare_state_t));
}

static void run_resource_query(void *context)
{

  sbp_prepare_state_t *prep_state = context;
  prep_state->current_index = 0;

  char *argv[] = {"ps", "--no-headers", "-e", "-o", "%p\t%C\t%c\t%a", "--sort=-pcpu", NULL};

  char buf[4096] = {0};
  int rc = run_with_stdin_file(NULL, "ps", argv, buf, sizeof(buf));

  if (rc != 0) {
    piksi_log(LOG_ERR | LOG_SBP, "error running 'ps' command: %s", strerror(errno));
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
#ifdef DEBUG_QUERY_CPU
  for (size_t i = 0; i < ITEM_COUNT; i++) {
    fprintf(stderr,
            "%zu: %d %d %s %s\n",
            i,
            query_context.items[i].pid,
            query_context.items[i].pcpu,
            query_context.items[i].thread_name,
            query_context.items[i].command_line);
  }
#endif
}

static bool prepare_resource_query_sbp(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context)
{

  sbp_prepare_state_t *prep_state = context;

  if (prep_state->current_index >= ITEM_COUNT) return false;

  *msg_type = SBP_MSG_LINUX_CPU_STATE;
  msg_linux_cpu_state_t *cpu_state = (msg_linux_cpu_state_t *)sbp_buf;

  u8 index = prep_state->current_index;

  cpu_state->index = index;
  cpu_state->pid = query_context.items[index].pid;
  cpu_state->pcpu = query_context.items[index].pcpu;

  strncpy(cpu_state->tname, query_context.items[index].thread_name, sizeof(cpu_state->tname));

  size_t cmdline_len = strlen(query_context.items[index].command_line);
  size_t msg_cmdline_len = SBP_PAYLOAD_SIZE_MAX - offsetof(msg_linux_cpu_state_t, cmdline);

  size_t copy_len = SWFT_MIN(cmdline_len, msg_cmdline_len);

  strncpy(cpu_state->cmdline, query_context.items[index].command_line, copy_len);
  *len = (u8)(sizeof(msg_linux_cpu_state_t) + copy_len);

  ++prep_state->current_index;

  return true;
}

static void teardown_resource_query(void **context)
{

  assert(context != NULL && *context != NULL);

  free(*context);
  *context = NULL;
}

resq_interface_t query_cpu = {
  .init = init_resource_query,
  .run_query = run_resource_query,
  .prepare_sbp = prepare_resource_query_sbp,
  .teardown = teardown_resource_query,
};

static __attribute__((constructor)) void register_cpu_query()
{
  resq_register(&query_cpu);
}
