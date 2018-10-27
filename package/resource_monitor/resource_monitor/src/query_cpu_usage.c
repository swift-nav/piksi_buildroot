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
#include "resmon_common.h"

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
  STATE_PID = 0,
  STATE_PCPU,
  STATE_THREAD_NAME,
  STATE_COMMAND_LINE,
  STATE_DONE,
  STATE_COUNT = STATE_DONE,
};

static bool parse_ps_cpu_line(const char *line, const size_t item_index)
{
  double pcpu_double = 0.0;

  line_spec_t line_specs[STATE_COUNT] = {
    [STATE_PID] =
      (line_spec_t){
        .type = FT_U16,
        .dst.u16 = &query_context.items[item_index].pid,
        .desc = "pid",
        .next = STATE_PCPU,
      },
    [STATE_PCPU] =
      (line_spec_t){
        .type = FT_F64,
        .dst.f64 = &pcpu_double,
        .desc = "pcpu",
        .next = STATE_THREAD_NAME,
      },
    [STATE_THREAD_NAME] =
      (line_spec_t){
        .type = FT_STR,
        .dst.str = query_context.items[item_index].thread_name,
        .buflen = sizeof(query_context.items[0].thread_name),
        .desc = "thread name",
        .next = STATE_COMMAND_LINE,
      },
    [STATE_COMMAND_LINE] =
      (line_spec_t){
        .type = FT_STR,
        .dst.str = query_context.items[item_index].command_line,
        .buflen = sizeof(query_context.items[0].command_line),
        .desc = "command line",
        .next = STATE_DONE,
      },
  };

  bool parse_success = parse_ps_line(line, STATE_PID, STATE_DONE, line_specs);

  if (!parse_success) return false;

  query_context.items[item_index].pcpu = (u8)((1u << (sizeof(u8) * 8)) * (pcpu_double / 100.0));

  return true;
}

static void *init_resource_query()
{
  return calloc(1, sizeof(sbp_prepare_state_t));
}

static const char *describe_query(void)
{
  return "CPU usage";
}

static void run_resource_query(void *context)
{

  sbp_prepare_state_t *prep_state = context;
  prep_state->current_index = 0;

  const char *argv[] = {"ps", "--no-headers", "-e", "-o", "%p\t%C\t%c\t%a", "--sort=-pcpu", NULL};

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

    if (!parse_ps_cpu_line(line, item_index++)) {
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

static resq_interface_t query_descriptor = {
  .priority = RESQ_PRIORITY_1,
  .init = init_resource_query,
  .describe = describe_query,
  .read_property = NULL,
  .run_query = run_resource_query,
  .prepare_sbp = prepare_resource_query_sbp,
  .teardown = teardown_resource_query,
};

static __attribute__((constructor)) void register_cpu_query()
{
  resq_register(&query_descriptor);
}
