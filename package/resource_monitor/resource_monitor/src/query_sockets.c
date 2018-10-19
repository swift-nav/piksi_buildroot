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
#include <search.h>

#include <libsbp/linux.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/runit.h>

#include "resource_monitor.h"
#include "resource_query.h"
#include "resmon_common.h"

#define LAST_ITEM_INDEX 9
#define MAX_PROCESS_COUNT 1024

#define DEBUG_QUERY_SOCKETS

typedef enum {
  ST_NL = 0x01,
  ST_U_STR = 0x02,
  ST_U_DGR = 0x04,
  ST_TCP = 0x10,
  ST_UDP = 0x20

} socket_types_t;

typedef struct {

  u16 pid;

  u16 socket_count;
  socket_types_t socket_types;

} socket_tab;

typedef struct {

  int item_index;
  struct hsearch_data socket_tabs;

} resq_state_t;

enum {
  STATE_SOCKET_TYPE = 0,
  STATE_SOCKET_STATE,
  STATE_RECV_Q,
  STATE_SEND_Q,
  STATE_LOCAL_ADDR,
  STATE_REMOTE_ADDR,
  STATE_EXTRA_INFO,
  STATE_DONE,
  STATE_COUNT = STATE_DONE + 1,
};

static bool parse_ss_line(resq_state_t *state, const char *line)
{
  char socket_type[32] = {0};
  char socket_state[32] = {0};

  u32 recv_q = 0;
  u32 send_q = 0;

  char local_addr[128] = {0};
  char remote_addr[128] = {0};

  char extra_info[256] = {0};

  line_spec_t line_specs[STATE_COUNT] = {[STATE_SOCKET_TYPE] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = socket_type,
                                             .buflen = sizeof(socket_type),
                                             .desc = "socket type",
                                             .next = STATE_SOCKET_STATE,
                                           },
                                         [STATE_SOCKET_STATE] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = socket_state,
                                             .buflen = sizeof(socket_state),
                                             .desc = "socket state",
                                             .next = STATE_RECV_Q,
                                           },
                                         [STATE_RECV_Q] =
                                           (line_spec_t){
                                             .type = FT_U32,
                                             .dst.u32 = &recv_q,
                                             .desc = "receive queue",
                                             .next = STATE_SEND_Q,
                                           },
                                         [STATE_SEND_Q] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.u32 = &send_q,
                                             .desc = "send queue",
                                             .next = STATE_LOCAL_ADDR,
                                           },
                                         [STATE_LOCAL_ADDR] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = local_addr,
                                             .buflen = sizeof(local_addr),
                                             .desc = "local address",
                                             .next = STATE_REMOTE_ADDR,
                                           },
                                         [STATE_REMOTE_ADDR] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = remote_addr,
                                             .buflen = sizeof(remote_addr),
                                             .desc = "remote address",
                                             .next = STATE_EXTRA_INFO,
                                           },
                                         [STATE_EXTRA_INFO] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = extra_info,
                                             .buflen = sizeof(extra_info),
                                             .desc = "extra info",
                                             .next = STATE_DONE,
                                           },
                                         [STATE_DONE] = (line_spec_t){
                                           .desc = "done",
                                         }};

  if (!parse_tab_line(line, STATE_SOCKET_TYPE, STATE_DONE, line_specs, " ")) {
    return false;
  }

  /* TODO */
  (void)state;
#ifdef DEBUG_QUERY_SOCKETS
  PK_LOG_ANNO(LOG_DEBUG,
              "type: %s; state: %s; recvq: %d; sendq: %d; local: %s; remote: %s; extra: %s",
              socket_type,
              socket_state,
              recv_q,
              send_q,
              local_addr,
              remote_addr,
              extra_info);
#endif

  return true;
}

static void run_resource_query(void *context)
{
  char output_buffer[4096] = {0};
  resq_state_t *state = context;

  run_command_t run_config = {.input = NULL,
                              .context = NULL,
                              .argv = (const char *[]){"ss", "-a", "-n", "-p", "-e", NULL},
                              .buffer = output_buffer,
                              .length = sizeof(output_buffer) - 1,
                              .func = NESTED_FN(ssize_t, (char *buffer, size_t length, void *ctx), {
                                (void)ctx;
                                buffer[length] = '\0';
#ifdef DEBUG_QUERY_SOCKETS
                                PK_LOG_ANNO(LOG_DEBUG, "length: %d, buffer: %s", length, buffer);
#endif
                                line_fn_t line_func = NESTED_FN(bool, (const char *line), {
#ifdef DEBUG_QUERY_SOCKETS
                                  PK_LOG_ANNO(LOG_DEBUG, "ss line: %s", line);
#endif
                                  if (!parse_ss_line(state, line)) {
                                    PK_LOG_ANNO(LOG_WARNING, "failed to parse 'ss' line: %s", line);
                                  }
                                  return true;
                                });
                                return (ssize_t)foreach_line(buffer, line_func);
                              })};

  if (run_command(&run_config) != 0) {
    PK_LOG_ANNO(LOG_ERR, "ss command failed: %s (errno: %d)", strerror(errno), errno);
    return;
  }

  /* TODO */
  (void)state;
}

static bool query_sockets_prepare(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context)
{
  resq_state_t *state = context;

  if (state->item_index > LAST_ITEM_INDEX) return false;

  (void)msg_type;
  (void)len;
  (void)sbp_buf;

  ++state->item_index;

  return true;
}

static void *init_resource_query()
{
  resq_state_t *state = calloc(1, sizeof(resq_state_t));

  char output_buffer[4096];
  int NESTED_AXX(line_count) = 0;

  run_command_t run_config = {.input = NULL,
                              .context = NULL,
                              .argv = (const char *[]){"ps", "--no-headers", "-a", "-e", NULL},
                              .buffer = output_buffer,
                              .length = sizeof(output_buffer) - 1,
                              .func = NESTED_FN(ssize_t, (char *buffer, size_t length, void *ctx), {
#ifdef DEBUG_QUERY_SOCKETS
                                PK_LOG_ANNO(LOG_DEBUG,
                                            "counting ps lines in buffer of size %d",
                                            length);
#endif
                                (void)ctx;
                                buffer[length] = '\0';
                                int _line_count = count_sz_lines(buffer);
                                if (_line_count < 0) {
                                  PK_LOG_ANNO(LOG_ERR, "counting ps lines failed");
                                  line_count = -1;
                                  return -1;
                                }
                                line_count += _line_count;
                                return (ssize_t)length;
                              })};

  if (run_command(&run_config) != 0) {
    PK_LOG_ANNO(LOG_ERR, "ps command failed: %s (errno: %d)", strerror(errno), errno);
    goto error;
  }

  if (line_count > MAX_PROCESS_COUNT) {
    PK_LOG_ANNO(LOG_ERR,
                "detected more than %d processes on the system: %d",
                MAX_PROCESS_COUNT,
                line_count);
    goto error;
  } else {
#ifdef DEBUG_QUERY_SOCKETS
    PK_LOG_ANNO(LOG_ERR, "detected %d processes on the system", line_count);
#endif
  }

  if (hcreate_r(MAX_PROCESS_COUNT, &state->socket_tabs) == 0) {
    PK_LOG_ANNO(LOG_ERR, "hash creation failed");
    goto error;
  }

  return state;

error:
  free(state);
  return NULL;
}

static const char *describe_query(void)
{
  return "sockets";
}

static void teardown_resource_query(void **context)
{
  if (context == NULL || *context == NULL) return;

  resq_state_t *state = *context;

  free(state);
  *context = NULL;
}

static resq_interface_t query_descriptor = {
  .init = init_resource_query,
  .describe = describe_query,
  .run_query = run_resource_query,
  .prepare_sbp = query_sockets_prepare,
  .teardown = teardown_resource_query,
};

static __attribute__((constructor)) void register_cpu_query()
{
  resq_register(&query_descriptor);
}
