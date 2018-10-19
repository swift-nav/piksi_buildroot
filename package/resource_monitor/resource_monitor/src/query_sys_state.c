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
#include <libpiksi/runit.h>

#include "resource_monitor.h"
#include "resource_query.h"
#include "resmon_common.h"

#define EXTRACE_LOG "/var/log/extrace.log"

typedef struct {

  int sent_sbp;

  unsigned long procs_started;
  unsigned long procs_exitted;

  runit_config_t runit_cfg;

  double pcpu_used;
  unsigned long mem_used;

  unsigned long mem_total;

} prep_state_t;

static void s_runner(runner_t **r)
{
  if (r == NULL || *r == NULL) return;
  *r = (*r)->destroy(*r);
};

enum {
  STATE_PCPU = 0,
  STATE_VSZ,
  STATE_DONE,
  STATE_COUNT = STATE_DONE,
};

static bool parse_ps_cpu_mem_line(const char *line, prep_state_t *state)
{
  double pcpu = 0;
  u32 vsz = 0;

  line_spec_t line_specs[STATE_COUNT] = {
    [STATE_PCPU] =
      (line_spec_t){
        .type = FT_F64,
        .dst.f64 = &pcpu,
        .desc = "pcpu",
        .next = STATE_VSZ,
      },
    [STATE_VSZ] =
      (line_spec_t){
        .type = FT_U32,
        .dst.u32 = &vsz,
        .desc = "vsz",
        .next = STATE_DONE,
      },
  };

  bool parse_success = parse_ps_line(line, STATE_PCPU, STATE_DONE, line_specs);
  if (!parse_success) return false;

  state->pcpu_used += pcpu;
  state->mem_used += vsz;

  return true;
}

static void run_resource_query(void *context)
{
  prep_state_t *state = context;
  stop_runit_service(&state->runit_cfg);

  state->sent_sbp = 0;
  state->pcpu_used = 0;
  state->mem_used = 0;

  state->procs_started = 0;
  {
    runner_t *SCRUB(r, s_runner) = create_runner();
    r = r->cat(r, EXTRACE_LOG);
    r = r->pipe(r);
    r = r->call(r, "grep", (const char *const[]){"grep", "-c", "^[0-9][0-9]*[+] ", NULL});

    r = r->wait(r);

    if (r->is_nil(r)) {
      PK_LOG_ANNO(LOG_ERR, "grep call failed: %d", r->exit_code);
    } else {
      if (!strtoul_all(10, r->stdout_buffer, &state->procs_started)) {
        PK_LOG_ANNO(LOG_ERR,
                    "grep returned invalid value: '%s' (exit: %d)",
                    r->stdout_buffer,
                    r->exit_code);
      }
    }
  }

  state->procs_exitted = 0;
  {
    runner_t *SCRUB(r, s_runner) = create_runner();
    r = r->cat(r, EXTRACE_LOG);
    r = r->pipe(r);
    r = r->call(r, "grep", (const char *const[]){"grep", "-c", "^[0-9][0-9]*[-] ", NULL});

    r = r->wait(r);

    if (r->is_nil(r)) {
      PK_LOG_ANNO(LOG_ERR, "grep call failed: %d", r->exit_code);
    } else {
      if (!strtoul_all(10, r->stdout_buffer, &state->procs_exitted)) {
        PK_LOG_ANNO(LOG_ERR,
                    "grep returned invalid value: '%s' (exit: %d)",
                    r->stdout_buffer,
                    r->exit_code);
      }
    }
  }

  const char *argv[] = {"ps", "--no-headers", "-e", "-o", "%C\t%z", NULL};

  char buf[16 * 1024] = {0};
  int rc = run_with_stdin_file(NULL, "ps", argv, buf, sizeof(buf));

  if (rc != 0) {
    PK_LOG_ANNO(LOG_ERR | LOG_SBP, "error running 'ps' command: %s", strerror(errno));
  }
  foreach_line(buf, NESTED_FN(bool, (const char *line), {
                 if (!parse_ps_cpu_mem_line(line, state)) {
                   return false;
                 }
                 return true;
               }));

  start_runit_service(&state->runit_cfg);
}

static bool prepare_resource_query_sbp(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context)
{
  prep_state_t *state = context;

  *msg_type = SBP_MSG_LINUX_SYS_STATE;
  msg_linux_sys_state_t *sys_state = (msg_linux_sys_state_t *)sbp_buf;

  *len = (u8)(sizeof(msg_linux_sys_state_t));

  sys_state->procs_starting = (u16)state->procs_started;
  sys_state->procs_stopping = (u16)state->procs_exitted;

  sys_state->mem_total = (u16)(state->mem_total / 1024);

  sys_state->pcpu = (u8)((1u << (sizeof(u8) * 8)) * (state->pcpu_used / 100.0));
  sys_state->pmem =
    (u8)((1u << (sizeof(u8) * 8)) * ((double)state->mem_used / sys_state->mem_total));

  return state->sent_sbp++ == 0;
}

static void *init_resource_query()
{
  prep_state_t *state = calloc(1, sizeof(prep_state_t));

  state->sent_sbp = 0;

  state->procs_started = 0;
  state->procs_exitted = 0;

  state->mem_total = fetch_mem_total();

  state->runit_cfg = (runit_config_t){
    .service_dir = RUNIT_SERVICE_DIR,
    .service_name = "extrace",
    .command_line = "extrace -f -t -o " EXTRACE_LOG,
    .restart = false,
  };

  start_runit_service(&state->runit_cfg);

  return state;
}

static const char *describe_query(void)
{
  return "system state";
}

static void teardown_resource_query(void **context)
{
  if (context == NULL || *context == NULL) return;

  prep_state_t *state = *context;

  stop_runit_service(&state->runit_cfg);
  free(state);

  *context = NULL;
}

static resq_interface_t query_descriptor = {
  .init = init_resource_query,
  .describe = describe_query,
  .run_query = run_resource_query,
  .prepare_sbp = prepare_resource_query_sbp,
  .teardown = teardown_resource_query,
};

static __attribute__((constructor)) void register_cpu_query()
{
  resq_register(&query_descriptor);
}
