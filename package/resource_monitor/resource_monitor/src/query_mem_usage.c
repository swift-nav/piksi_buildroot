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

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <libsbp/linux.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "resource_query.h"
#include "resmon_common.h"

#define ITEM_COUNT (10u)

#define THREAD_NAME_MAX (16u)
#define COMMAND_LINE_MAX (256u)

//#define DEBUG_QUERY_MEM

typedef struct {
  u32 total_memory;
  u8 current_index;
} resq_state_t;

typedef struct {
  u16 pid;
  u8 pmem;
  char thread_name[THREAD_NAME_MAX];
  char command_line[COMMAND_LINE_MAX];
} query_item_t;

static struct {
  query_item_t items[ITEM_COUNT];
} query_context;

enum {
  STATE_PID = 0,
  STATE_VSZ,
  STATE_THREAD_NAME,
  STATE_COMMAND_LINE,
  STATE_DONE,
  STATE_COUNT = STATE_DONE,
};

static bool parse_ps_mem_line(const char *line, const size_t item_index, resq_state_t *state)
{
  u32 vsz = 0;

  line_spec_t line_specs[STATE_COUNT] = {
    [STATE_PID] =
      (line_spec_t){
        .type = FT_U16,
        .dst.u16 = &query_context.items[item_index].pid,
        .desc = "pid",
        .next = STATE_VSZ,
      },
    [STATE_VSZ] =
      (line_spec_t){
        .type = FT_U32,
        .dst.u32 = &vsz,
        .desc = "vsz",
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

  query_context.items[item_index].pmem =
    (u8)((1u << (sizeof(u8) * 8)) * ((double)vsz / state->total_memory));

  return true;
}

static void run_resource_query(void *context)
{
  resq_state_t *prep_state = context;
  prep_state->current_index = 0;

  /* TODO: use regular ps output with %MEM, not the vsz figure which seems to calculate the
   * wrong percentage of system memory used at certain points.
   */
  const char *argv[] = {"ps", "--no-headers", "-e", "-o", "%p\t%z\t%c\t%a", "--sort=-vsz", NULL};

  char buf[4096] = {0};
  int rc = run_with_stdin_file(NULL, "ps", argv, buf, sizeof(buf));

  if (rc != 0) {
    piksi_log(LOG_ERR | LOG_SBP, "error running 'ps' command: %s", strerror(errno));
    return;
  }

  size_t NESTED_AXX(item_index) = 0;
  memset(&query_context, 0, sizeof(query_context));

  ssize_t consumed = foreach_line(buf, NULL, NESTED_FN(bool, (const char *line), {
                                    if (item_index >= ITEM_COUNT
                                        || !parse_ps_mem_line(line, item_index++, prep_state)) {
                                      return false;
                                    }
                                    return true;
                                  }));

  if (consumed < 0) {
    piksi_log(LOG_ERR | LOG_SBP, "error parsing 'ps' data");
    return;
  }

#ifdef DEBUG_QUERY_MEM
  for (size_t i = 0; i < ITEM_COUNT; i++) {
    fprintf(stderr,
            "%zu: %d %d %s %s (%s:%d)\n",
            i,
            query_context.items[i].pid,
            query_context.items[i].pmem,
            query_context.items[i].thread_name,
            query_context.items[i].command_line,
            __FILE__,
            __LINE__);
  }
#endif
}

static bool prepare_resource_query_sbp(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context)
{
  resq_state_t *prep_state = context;

  if (prep_state->current_index >= ITEM_COUNT) return false;

  *msg_type = SBP_MSG_LINUX_MEM_STATE;
  msg_linux_mem_state_t *mem_state = (msg_linux_mem_state_t *)sbp_buf;

  u8 index = prep_state->current_index;

  mem_state->index = index;
  mem_state->pid = query_context.items[index].pid;
  mem_state->pmem = query_context.items[index].pmem;

  strncpy(mem_state->tname, query_context.items[index].thread_name, sizeof(mem_state->tname));

  size_t cmdline_len = strlen(query_context.items[index].command_line);
  size_t msg_cmdline_len = SBP_PAYLOAD_SIZE_MAX - offsetof(msg_linux_mem_state_t, cmdline);

  size_t copy_len = SWFT_MIN(cmdline_len, msg_cmdline_len);

  strncpy(mem_state->cmdline, query_context.items[index].command_line, copy_len);
  *len = (u8)(sizeof(msg_linux_mem_state_t) + copy_len);

  ++prep_state->current_index;

  return true;
}

static void *init_resource_query()
{
  resq_state_t *state = calloc(1, sizeof(resq_state_t));
  state->total_memory = (u32)fetch_mem_total();

  if (state->total_memory == 0) goto error;

  return state;

error:
  free(state);
  return NULL;
}

static const char *describe_query(void)
{
  return "system state";
}

static void teardown_resource_query(void **context)
{
  if (context == NULL || *context == NULL) return;

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
