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
#include <libpiksi/table.h>

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

  char pid_str[16];
  u16 pid;

  u16 socket_count;
  socket_types_t socket_types;

} pid_entry_t;

MAKE_TABLE_WRAPPER(pid, pid_entry_t)

typedef struct {

  int item_index;
  size_t pid_count;

  table_t *pid_table;

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
  STATE_COUNT,
};

enum {
  UDS_STATE_SOCKET_TYPE = 0,
  UDS_STATE_SOCKET_STATE,
  UDS_STATE_RECV_Q,
  UDS_STATE_SEND_Q,
  UDS_STATE_LOCAL_ADDR1,
  UDS_STATE_LOCAL_ADDR2,
  UDS_STATE_REMOTE_ADDR1,
  UDS_STATE_REMOTE_ADDR2,
  UDS_STATE_EXTRA_INFO,
  UDS_STATE_DONE,
  UDS_STATE_COUNT,
};

static void process_ss_entry(resq_state_t *state,
                             const char *socket_type,
                             const char *socket_state,
                             const u32 recv_q,
                             const u32 send_q,
                             const char *local_addr,
                             const char *remote_addr,
                             const char *pid_str)
{
  pid_entry_t *entry = pid_table_get(state->pid_table, pid_str);

  if (entry == NULL) {
#ifdef DEBUG_QUERY_SOCKETS
    PK_LOG_ANNO(LOG_ERR, "creating new entry for '%s'", pid_str);
#endif
    pid_entry_t new_entry = {0};
    int s = snprintf(new_entry.pid_str, sizeof(entry->pid_str), "%s", pid_str);

    if (s < 0 || (size_t)s >= sizeof(entry->pid_str)) {
      PK_LOG_ANNO(LOG_ERR, "failed to copy pid string");
      return;
    }

    unsigned long pid = 0;
    if (!strtoul_all(10, pid_str, &pid)) {
      PK_LOG_ANNO(LOG_ERR, "failed to convert pid str to number: %s", strerror(errno));
      return;
    }

    new_entry.pid = (u16)pid;

    entry = calloc(1, sizeof(pid_entry_t));
    *entry = new_entry;

    bool added = pid_table_put(state->pid_table, pid_str, entry);

    if (!added) {
      PK_LOG_ANNO(LOG_ERR, "failed add entry for pid '%s'", pid_str);
      free(entry);
      return;
    }

  } else {
#ifdef DEBUG_QUERY_SOCKETS
    PK_LOG_ANNO(LOG_ERR, "found existing entry for %s", pid_str);
#endif

    /* TODO */
    (void)socket_type;
    (void)socket_state;
    (void)recv_q;
    (void)send_q;
    (void)local_addr;
    (void)remote_addr;
  }
}

static bool parse_socket_common(resq_state_t *state,
                                const char *socket_type,
                                const char *socket_state,
                                const u32 recv_q,
                                const u32 send_q,
                                const char *local_addr,
                                const char *remote_addr,
                                const char *extra_info)
{
  char *pid_str = NULL;
  char *pid_eq = strstr(extra_info, "pid=");

  if (pid_eq == NULL) {
    PK_LOG_ANNO(LOG_WARNING, "extra info did not have pid information: %s", extra_info);
    return false;
  }

  int items = sscanf(pid_eq, "pid=%m[^,]", &pid_str);

  if (items == 0) {
    PK_LOG_ANNO(LOG_WARNING, "extra info did not have pid information: %s", extra_info);
    return false;
  }

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

  assert(pid_str != NULL);

  process_ss_entry(state,
                   socket_type,
                   socket_state,
                   recv_q,
                   send_q,
                   local_addr,
                   remote_addr,
                   pid_str);

  free(pid_str);

  return true;
}

static bool parse_uds_socket_line(resq_state_t *state, const char *line)
{
  char socket_type[32] = {0};
  char socket_state[32] = {0};

  u32 recv_q = 0;
  u32 send_q = 0;

  char local_addr1[128] = {0};
  char local_addr2[128] = {0};
  char remote_addr1[128] = {0};
  char remote_addr2[128] = {0};

  char extra_info[256] = {0};

  line_spec_t line_specs[UDS_STATE_COUNT] = {[UDS_STATE_SOCKET_TYPE] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = socket_type,
                                             .buflen = sizeof(socket_type),
                                             .desc = "socket type",
                                             .next = UDS_STATE_SOCKET_STATE,
                                           },
                                         [UDS_STATE_SOCKET_STATE] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = socket_state,
                                             .buflen = sizeof(socket_state),
                                             .desc = "socket state",
                                             .next = UDS_STATE_RECV_Q,
                                           },
                                         [UDS_STATE_RECV_Q] =
                                           (line_spec_t){
                                             .type = FT_U32,
                                             .dst.u32 = &recv_q,
                                             .desc = "receive queue",
                                             .next = UDS_STATE_SEND_Q,
                                           },
                                         [UDS_STATE_SEND_Q] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.u32 = &send_q,
                                             .desc = "send queue",
                                             .next = UDS_STATE_LOCAL_ADDR1,
                                           },
                                         [UDS_STATE_LOCAL_ADDR1] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = local_addr1,
                                             .buflen = sizeof(local_addr1),
                                             .desc = "local address 1",
                                             .next = UDS_STATE_LOCAL_ADDR2,
                                           },
                                         [UDS_STATE_LOCAL_ADDR2] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = local_addr2,
                                             .buflen = sizeof(local_addr2),
                                             .desc = "local address 2",
                                             .next = UDS_STATE_REMOTE_ADDR1,
                                           },
                                         [UDS_STATE_REMOTE_ADDR1] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = remote_addr1,
                                             .buflen = sizeof(remote_addr1),
                                             .desc = "remote address 1",
                                             .next = UDS_STATE_REMOTE_ADDR2,
                                           },
                                         [UDS_STATE_REMOTE_ADDR2] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = remote_addr1,
                                             .buflen = sizeof(remote_addr1),
                                             .desc = "remote address 2",
                                             .next = UDS_STATE_EXTRA_INFO,
                                           },
                                         [UDS_STATE_EXTRA_INFO] =
                                           (line_spec_t){
                                             .type = FT_STR,
                                             .dst.str = extra_info,
                                             .buflen = sizeof(extra_info),
                                             .desc = "extra info",
                                             .next = UDS_STATE_DONE,
                                           },
                                         [UDS_STATE_DONE] = (line_spec_t){
                                           .desc = "done",
                                         }};

  if (!parse_tab_line(line, UDS_STATE_SOCKET_TYPE, UDS_STATE_DONE, line_specs, " ")) {
    return false;
  }

  return parse_socket_common(state, 
                             socket_type,
                             socket_state,
                             recv_q,
                             send_q,
                             local_addr1,
                             remote_addr2,
                             extra_info);
}

static bool parse_ext_socket_line(resq_state_t *state, const char *line)
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

  return parse_socket_common(state, 
                             socket_type,
                             socket_state,
                             recv_q,
                             send_q,
                             local_addr,
                             remote_addr,
                             extra_info);
}

static void destroy_table_entry(table_t *table, void **entry)
{
  (void)table;

  pid_entry_t *pid_entry = *entry;
  free(pid_entry);

  *entry = NULL;
}

static void run_resource_query(void *context)
{
  char output_buffer[8192] = {0};
  char leftover_buffer[4096] = {0};

  leftover_t NESTED_AXX(leftover) = { .buf = leftover_buffer };
  size_t NESTED_AXX(no_line_count) = 0;

  resq_state_t *state = context;

  if (state->pid_table != NULL) {
    table_destroy(&state->pid_table);
  }

  state->pid_table = table_create(MAX_PROCESS_COUNT, destroy_table_entry);

  if (state->pid_table == NULL) {
    PK_LOG_ANNO(LOG_ERR, "table creation failed");
    return;
  }

  line_fn_t line_func = NESTED_FN(bool, (const char *line), {
//#ifdef DEBUG_QUERY_SOCKETS
    PK_LOG_ANNO(LOG_DEBUG, "ss line: '%s'", line);
//#endif
    if (strstr(line, "nl") == line) {
      PK_LOG_ANNO(LOG_DEBUG, "Skipping nl socket line");
      return true;
    }

    if (strstr(line, "u_str") == line || strstr(line, "u_dgr")) {
      if (!parse_uds_socket_line(state, line)) {
        PK_LOG_ANNO(LOG_WARNING, "failed to parse 'ss' line: %s", line);
      }
    } else {
      if (!parse_ext_socket_line(state, line)) {
        PK_LOG_ANNO(LOG_WARNING, "failed to parse 'ss' line: %s", line);
      }
    }

    return true;
  });

  buffer_fn_t buffer_fn = NESTED_FN(ssize_t, (char *buffer, size_t length, void *ctx), {
    (void)ctx;
    buffer[length] = '\0';

    char filename[] = "/tmp/resmond.XXXXXX";
    int fd = mkstemp(filename);

    write(fd, buffer, length);
    close(fd);
//#ifdef DEBUG_QUERY_SOCKETS
    PK_LOG_ANNO(LOG_DEBUG, "length: %d, filename: %s, buffer: <<%s>>", length, filename, buffer);
//#endif

    ssize_t consumed = foreach_line(buffer, &leftover, line_func);

    if (consumed < 0) {
      PK_LOG_ANNO(LOG_WARNING, "there was an error parsing the 'ss' output");
      return -1;
    }

    if (leftover.line_count == 0 && ++no_line_count >= 2) {
      PK_LOG_ANNO(LOG_WARNING, "command output had no newlines");
      return -1;
    }

    if (leftover.size > 0) {
      PK_LOG_ANNO(LOG_DEBUG, "leftover: '%s'", leftover.buf);
    }

    no_line_count = 0;

    return (ssize_t)length;
  });

  run_command_t run_config = {.input = NULL,
                              .context = NULL,
                              .argv = (const char *[]){"sudo", "/sbin/ss", "-a", "-n", "-p", NULL},
                              .buffer = output_buffer,
                              .length = sizeof(leftover_buffer) - 1,
                              .func = buffer_fn};

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

  state->pid_table = NULL;

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

  if (state->pid_table != NULL) {
    table_destroy(&state->pid_table);
  }

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
