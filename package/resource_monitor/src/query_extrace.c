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

#define EXTRACE_LOG "/var/log/extrace.log"

typedef struct {

  int sent_sbp;

  unsigned long procs_started;
  unsigned long procs_exitted;

  runit_config_t runit_cfg;

} prep_state_t;

static void run_resource_query(void *context)
{
  prep_state_t *state = context;
  stop_runit_service(&state->runit_cfg);

  state->sent_sbp = 0;

  state->procs_started = 0;
  {
    runner_t *r = create_runner();
    r = r->cat(r, EXTRACE_LOG);
    r = r->pipe(r);
    r = r->call(r, "grep", (const char *const[]){"grep", "-c", "^[0-9][0-9]*[+] ", NULL});

    r = r->wait(r);

    if (r->is_nil(r)) {
      piksi_log(LOG_ERR,
                "%s: grep call failed: %d (%s:%d)",
                __FUNCTION__,
                r->exit_code,
                __FILE__,
                __LINE__);
      return;
    }

    if (!strtoul_all(10, r->stdout_buffer, &state->procs_started)) {
      piksi_log(LOG_ERR,
                "%s: grep returned invalid value: '%s' (exit: %d) (%s:%d)",
                __FUNCTION__,
                r->stdout_buffer,
                r->exit_code,
                __FILE__,
                __LINE__);
      return;
    }
  }

  state->procs_exitted = 0;
  {
    runner_t *r = create_runner();
    r = r->cat(r, EXTRACE_LOG);
    r = r->pipe(r);
    r = r->call(r, "grep", (const char *const[]){"grep", "-c", "^[0-9][0-9]*[-] ", NULL});

    r = r->wait(r);

    if (r->is_nil(r)) {
      piksi_log(LOG_ERR,
                "%s: grep call failed: %d (%s:%d)",
                __FUNCTION__,
                r->exit_code,
                __FILE__,
                __LINE__);
      return;
    }

    if (!strtoul_all(10, r->stdout_buffer, &state->procs_exitted)) {
      piksi_log(LOG_ERR,
                "%s: grep returned invalid value: '%s' (exit: %d) (%s:%d)",
                __FUNCTION__,
                r->stdout_buffer,
                r->exit_code,
                __FILE__,
                __LINE__);
      return;
    }
  }
}

static bool prepare_resource_query_sbp(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context)
{
  prep_state_t *state = context;

  *msg_type = SBP_MSG_LINUX_SYS_STATE_SUMMARY;
  msg_linux_sys_state_summary_t *sys_state_summary = (msg_linux_sys_state_summary_t *)sbp_buf;

  *len = (u8)(sizeof(msg_linux_sys_state_summary_t));

  sys_state_summary->procs_starting = (u16)state->procs_started;
  sys_state_summary->procs_stopping = (u16)state->procs_exitted;

  return state->sent_sbp++ == 0;
}

static void *init_resource_query()
{
  prep_state_t *state = calloc(1, sizeof(prep_state_t));

  state->sent_sbp = 0;

  state->procs_started = 0;
  state->procs_exitted = 0;

  state->runit_cfg = (runit_config_t){
    .service_dir = RUNIT_SERVICE_DIR,
    .service_name = "extrace",
    .command_line = "extrace -f -t -o " EXTRACE_LOG,
    .restart = false,
  };

  start_runit_service(&state->runit_cfg);

  return state;
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
  .run_query = run_resource_query,
  .prepare_sbp = prepare_resource_query_sbp,
  .teardown = teardown_resource_query,
};

static __attribute__((constructor)) void register_cpu_query()
{
  resq_register(&query_descriptor);
}
