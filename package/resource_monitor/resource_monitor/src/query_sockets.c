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

#include "query_sys_state.h"
#include "resource_monitor.h"
#include "resource_query.h"
#include "resmon_common.h"

#define ITEM_COUNT 10
#define MAX_PROCESS_COUNT 1024

//#define DEBUG_QUERY_SOCKETS_PS
//#define DEBUG_QUERY_SOCKETS_SS_OUTPUT
//#define DEBUG_QUERY_SOCKETS_TAB
//#define DEBUG_QUERY_SOCKETS_PARSE_COMMON

/* clang-format off */
typedef enum {
  ST_NONE      = 0x0000,
  ST_TCP       = 0x0001,
  ST_UDP       = 0x0002,
  ST_UNIX_STR  = 0x0004,
  ST_UNIX_DGR  = 0x0008,
  ST_UNIX_SEQ  = 0x0010,
  ST_NETLINK   = 0x0020,
  ST_UNKNOWN   = 0x8000,
} socket_types_t;
/* clang-format on */

/* clang-format off */
typedef enum {
  SS_ESTAB       = 0x0001,
  SS_SYN_SENT    = 0x0002,
  SS_SYN_RECV    = 0x0004,
  SS_FIN_WAIT1   = 0x0008,
  SS_FIN_WAIT2   = 0x0010,
  SS_TIME_WAIT   = 0x0020, 
  SS_CLOSED      = 0x0040,
  SS_CLOSE_WAIT  = 0x0080,
  SS_LAST_ACK    = 0x0100,
  SS_LISTEN      = 0x0200,
  SS_CLOSING     = 0x0400,
  SS_UNCONNECTED = 0x0800,
  SS_UNKNOWN     = 0x8000,
} socket_states_t;
/* clang-format on */

typedef enum {
  SEND_SOCKET_COUNTS_0,
  SEND_SOCKET_COUNTS_1,
  SEND_SOCKET_COUNTS_2,
  SEND_SOCKET_COUNTS_3,
  SEND_SOCKET_COUNTS_4,
  SEND_SOCKET_COUNTS_5,
  SEND_SOCKET_COUNTS_6,
  SEND_SOCKET_COUNTS_7,
  SEND_SOCKET_COUNTS_8,
  SEND_SOCKET_COUNTS_9,
  SEND_SOCKET_QUEUES_0,
  SEND_SOCKET_QUEUES_1,
  SEND_SOCKET_QUEUES_2,
  SEND_SOCKET_QUEUES_3,
  SEND_SOCKET_QUEUES_4,
  SEND_SOCKET_QUEUES_5,
  SEND_SOCKET_QUEUES_6,
  SEND_SOCKET_QUEUES_7,
  SEND_SOCKET_QUEUES_8,
  SEND_SOCKET_QUEUES_9,
  SEND_DONE,
} send_state_t;

typedef struct {

  char pid_str[16];
  u16 pid;

  u16 socket_count;

  socket_types_t socket_types;
  socket_states_t socket_states;

  u32 total_queue;

  u32 send_queue;
  u32 recv_queue;

  u32 max_queue;

  char max_queue_address[64];
  char command_line[256];

} pid_entry_t;

MAKE_TABLE_WRAPPER(pid, pid_entry_t)

typedef struct {

  send_state_t send_state;
  size_t pid_count;

  table_t *pid_table;

  const char *top10_counts[ITEM_COUNT];
  const char *top10_queues[ITEM_COUNT];

} resq_state_t;

typedef struct pid_to_count {
  u16 count;
  const char *pid;
} pid_to_count_t;

static int compare_counts(const void *ptc1, const void *ptc2)
{
  const pid_to_count_t *pid_to_count1 = ptc1;
  const pid_to_count_t *pid_to_count2 = ptc2;
  return -((int)pid_to_count1->count - (int)pid_to_count2->count);
}

typedef struct pid_to_queue {
  u32 queue_depth;
  const char *pid;
} pid_to_queue_t;

static int compare_queues(const void *ptc1, const void *ptc2)
{
  const pid_to_queue_t *pid_to_queue1 = ptc1;
  const pid_to_queue_t *pid_to_queue2 = ptc2;
  return -((int)pid_to_queue1->queue_depth - (int)pid_to_queue2->queue_depth);
}

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
                             const char *socket_type_str,
                             const char *socket_state_str,
                             const u32 recv_q,
                             const u32 send_q,
                             const char *local_addr1,
                             const char *local_addr2,
                             const char *remote_addr1,
                             const char *remote_addr2,
                             const char *pid_str)
{
  pid_entry_t *entry = pid_table_get(state->pid_table, pid_str);

  if (entry == NULL) {
#ifdef DEBUG_QUERY_SOCKETS_TAB
    PK_LOG_ANNO(LOG_DEBUG, "creating new entry for '%s'", pid_str);
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
#ifdef DEBUG_QUERY_SOCKETS_TAB
    PK_LOG_ANNO(LOG_DEBUG, "found existing entry for %s", pid_str);
#endif
  }

  if (strcmp(socket_state_str, "ESTAB") == 0) {
    entry->socket_states |= SS_ESTAB;
  } else if (strcmp(socket_state_str, "LISTEN") == 0) {
    entry->socket_states |= SS_LISTEN;
  } else if (strcmp(socket_state_str, "UNCONN") == 0) {
    entry->socket_states |= SS_UNCONNECTED;
  } else if (strcmp(socket_state_str, "SYN-SENT") == 0) {
    entry->socket_states |= SS_SYN_SENT;
  } else if (strcmp(socket_state_str, "SYN-RECV") == 0) {
    entry->socket_states |= SS_SYN_RECV;
  } else if (strcmp(socket_state_str, "FIN-WAIT-1") == 0) {
    entry->socket_states |= SS_FIN_WAIT1;
  } else if (strcmp(socket_state_str, "FIN-WAIT-2") == 0) {
    entry->socket_states |= SS_FIN_WAIT2;
  } else if (strcmp(socket_state_str, "TIME-WAIT") == 0) {
    entry->socket_states |= SS_TIME_WAIT;
  } else if (strcmp(socket_state_str, "CLOSED") == 0) {
    entry->socket_states |= SS_CLOSED;
  } else if (strcmp(socket_state_str, "CLOSE-WAIT") == 0) {
    entry->socket_states |= SS_CLOSE_WAIT;
  } else if (strcmp(socket_state_str, "LAST-ACK") == 0) {
    entry->socket_states |= SS_LAST_ACK;
  } else if (strcmp(socket_state_str, "CLOSING") == 0) {
    entry->socket_states |= SS_CLOSING;
  } else {
    entry->socket_states |= SS_UNKNOWN;
    PK_LOG_ANNO(LOG_WARNING, "unknown socket state: %s", socket_state_str);
  }

  if (strcmp(socket_type_str, "u_str") == 0) {
    entry->socket_types |= ST_UNIX_STR;
  } else if (strcmp(socket_type_str, "u_dgr") == 0) {
    entry->socket_types |= ST_UNIX_DGR;
  } else if (strcmp(socket_type_str, "u_seq") == 0) {
    entry->socket_types |= ST_UNIX_SEQ;
  } else if (strcmp(socket_type_str, "tcp") == 0) {
    entry->socket_types |= ST_TCP;
  } else if (strcmp(socket_type_str, "udp") == 0) {
    entry->socket_types |= ST_UDP;
  } else {
    entry->socket_types |= ST_UNKNOWN;
    /* TODO: only report once per socket type */
    PK_LOG_ANNO(LOG_WARNING, "unknown socket type: %s", socket_type_str);
  }

  entry->socket_count++;

  entry->recv_queue += recv_q;
  entry->send_queue += send_q;

  u32 total_queue = recv_q + send_q;
  entry->total_queue += total_queue;

  if (total_queue > entry->max_queue) {
    bzero(entry->max_queue_address, sizeof(entry->max_queue_address));
    snprintf(entry->max_queue_address,
             sizeof(entry->max_queue_address) - 1,
             "%s,%s,%s,%s",
             local_addr1,
             local_addr2,
             remote_addr1,
             remote_addr2);
    entry->max_queue = total_queue;
  }

  /* TODO: need to get command line from /proc and copy into pid_state_t */
}

static bool parse_socket_common(resq_state_t *state,
                                const char *socket_type,
                                const char *socket_state,
                                const u32 recv_q,
                                const u32 send_q,
                                const char *local_addr1,
                                const char *local_addr2,
                                const char *remote_addr1,
                                const char *remote_addr2,
                                const char *extra_info)
{
  char *pid_str = NULL;
  char *pid_eq = strstr(extra_info, "pid=");

  if (pid_eq == NULL) {
    PK_LOG_ANNO(LOG_WARNING, "extra info did not have pid information: %s", extra_info);
    return false;
  }

  /* Forked processes will have multiple pid= entries in the 'ss' output,
   * it's should be sufficient to only report these sockets for the first
   * pid entry (which is probably the parent process).
   */
  int items = sscanf(pid_eq, "pid=%m[^,]", &pid_str);

  if (items == 0) {
    PK_LOG_ANNO(LOG_WARNING, "extra info did not have pid information: %s", extra_info);
    return false;
  }

#ifdef DEBUG_QUERY_SOCKETS_PARSE_COMMON
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
                   local_addr1,
                   local_addr2,
                   remote_addr1,
                   remote_addr2,
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
                             local_addr2,
                             remote_addr1,
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
                             "",
                             remote_addr,
                             "",
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

  leftover_t NESTED_AXX(leftover) = {.buf = leftover_buffer};
  size_t NESTED_AXX(no_line_count) = 0;

  resq_state_t *state = context;

  if (state->pid_table != NULL) {
    table_destroy(&state->pid_table);
  }

  state->send_state = SEND_SOCKET_COUNTS_0;
  state->pid_table = table_create(MAX_PROCESS_COUNT, destroy_table_entry);

  if (state->pid_table == NULL) {
    PK_LOG_ANNO(LOG_ERR, "table creation failed");
    return;
  }

  line_fn_t line_func = NESTED_FN(bool, (const char *line), {
#ifdef DEBUG_QUERY_SOCKETS_SS_OUTPUT
    PK_LOG_ANNO(LOG_DEBUG, "ss line: '%s'", line);
#endif
    if (strstr(line, "Netid") == line) {
#ifdef DEBUG_QUERY_SOCKETS_SS_OUTPUT
      PK_LOG_ANNO(LOG_DEBUG, "Skipping header line: '%s'", line);
#endif
      return true;
    }

    if (strstr(line, "nl") == line) {
#ifdef DEBUG_QUERY_SOCKETS_SS_OUTPUT
      PK_LOG_ANNO(LOG_DEBUG, "Skipping nl socket line: '%s'", line);
#endif
      return true;
    }

    if (strstr(line, "TIME-WAIT") != NULL) {
      PK_LOG_ANNO(LOG_DEBUG, "Skipping TIME-WAIT socket, not parsing 'ss' output: '%s'", line);
      return true;
    }

    if (strstr(line, "u_str") == line || strstr(line, "u_dgr") == line
        || strstr(line, "u_seq") == line) {
      if (!parse_uds_socket_line(state, line)) {
        PK_LOG_ANNO(LOG_WARNING, "failed to parse uds 'ss' line: %s", line);
      }
    } else {
      if (!parse_ext_socket_line(state, line)) {
        PK_LOG_ANNO(LOG_WARNING, "failed to parse ext 'ss' line: %s", line);
      }
    }
    /* TODO: need to process sockets in TIME-WAIT (and other states) differently, they don't
     * seem to have all the same info as other states... in particular, we should process
     * socket states separately from the socket ownership, and not worry about how many
     * fields a line has since we only need the first 2 for socket state (at least for
     * system wide socket states).
     */
    return true;
  });

  buffer_fn_t buffer_fn = NESTED_FN(ssize_t, (char *buffer, size_t length, void *ctx), {

    (void)ctx;

    /* foreach_line requires a null terminated string */
    buffer[length] = '\0';
#ifdef DEBUG_QUERY_SOCKETS_SS_OUTPUT
    PK_LOG_ANNO(LOG_DEBUG, "length: %d, buffer: <<%s>>", length, buffer);
#endif

    ssize_t consumed = foreach_line(buffer, &leftover, line_func);
    if (consumed < 0) {
      PK_LOG_ANNO(LOG_WARNING, "there was an error parsing the 'ss' output");
      return -1;
    }

    if (leftover.line_count == 0 && ++no_line_count >= 2) {
      PK_LOG_ANNO(LOG_WARNING, "command output had no newlines");
      return -1;
    }

#ifdef DEBUG_QUERY_SOCKETS_SS_OUTPUT
    if (leftover.size > 0) {
      PK_LOG_ANNO(LOG_DEBUG, "leftover: '%s'", leftover.buf);
    }
#endif
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

  size_t pid_count = table_count(state->pid_table);

  pid_to_count_t *NESTED_AXX(pid_to_count) = alloca(pid_count * sizeof(pid_to_count_t));
  pid_to_queue_t *NESTED_AXX(pid_to_queue) = alloca(pid_count * sizeof(pid_to_queue_t));

  table_foreach_key(state->pid_table,
                    NULL,
                    NESTED_FN(bool,
                              (table_t * table, const char *key, size_t index, void *context1),
                              {

                                (void)context1;

                                pid_entry_t *entry = pid_table_get(table, key);

                                pid_to_count[index].pid = key;
                                pid_to_count[index].count = entry->socket_count;

                                pid_to_queue[index].pid = key;
                                pid_to_queue[index].queue_depth = entry->total_queue;

                                return true;
                              }));

  qsort(pid_to_count, pid_count, sizeof(pid_to_count_t), compare_counts);
  qsort(pid_to_queue, pid_count, sizeof(pid_to_queue_t), compare_queues);

  for (size_t i = 0; i < ITEM_COUNT && i < pid_count; i++) {
    state->top10_counts[i] = pid_to_count[i].pid;
    state->top10_queues[i] = pid_to_queue[i].pid;
  }
}

static bool query_sockets_prepare(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context)
{
  resq_state_t *state = context;

  if (state->send_state == SEND_DONE) return false;

  size_t index = 0;

  switch (state->send_state) {
  case SEND_SOCKET_COUNTS_0:
    index = 0;
    state->send_state = SEND_SOCKET_COUNTS_1;
    break;
  case SEND_SOCKET_COUNTS_1:
    index = 1;
    state->send_state = SEND_SOCKET_COUNTS_2;
    break;
  case SEND_SOCKET_COUNTS_2:
    index = 2;
    state->send_state = SEND_SOCKET_COUNTS_3;
    break;
  case SEND_SOCKET_COUNTS_3:
    index = 3;
    state->send_state = SEND_SOCKET_COUNTS_4;
    break;
  case SEND_SOCKET_COUNTS_4:
    index = 4;
    state->send_state = SEND_SOCKET_COUNTS_5;
    break;
  case SEND_SOCKET_COUNTS_5:
    index = 5;
    state->send_state = SEND_SOCKET_COUNTS_6;
    break;
  case SEND_SOCKET_COUNTS_6:
    index = 6;
    state->send_state = SEND_SOCKET_COUNTS_7;
    break;
  case SEND_SOCKET_COUNTS_7:
    index = 7;
    state->send_state = SEND_SOCKET_COUNTS_8;
    break;
  case SEND_SOCKET_COUNTS_8:
    index = 8;
    state->send_state = SEND_SOCKET_COUNTS_9;
    break;
  case SEND_SOCKET_COUNTS_9:
    index = 9;
    state->send_state = SEND_DONE;
    break;
  case SEND_DONE: break;
  case SEND_SOCKET_QUEUES_0:
  case SEND_SOCKET_QUEUES_1:
  case SEND_SOCKET_QUEUES_2:
  case SEND_SOCKET_QUEUES_3:
  case SEND_SOCKET_QUEUES_4:
  case SEND_SOCKET_QUEUES_5:
  case SEND_SOCKET_QUEUES_6:
  case SEND_SOCKET_QUEUES_7:
  case SEND_SOCKET_QUEUES_8:
  case SEND_SOCKET_QUEUES_9:
  default: PK_LOG_ANNO(LOG_ERR, "Invalid state value: %d", state->send_state);
  }

  switch (state->send_state) {
  case SEND_SOCKET_COUNTS_0:
  case SEND_SOCKET_COUNTS_1:
  case SEND_SOCKET_COUNTS_2:
  case SEND_SOCKET_COUNTS_3:
  case SEND_SOCKET_COUNTS_4:
  case SEND_SOCKET_COUNTS_5:
  case SEND_SOCKET_COUNTS_6:
  case SEND_SOCKET_COUNTS_7:
  case SEND_SOCKET_COUNTS_8:
  case SEND_SOCKET_COUNTS_9: {
    pid_entry_t *entry = pid_table_get(state->pid_table, state->top10_counts[index]);
    *msg_type = SBP_MSG_LINUX_PROCESS_SOCKET_COUNTS;
    msg_linux_process_socket_counts_t *socket_counts = (msg_linux_process_socket_counts_t *)sbp_buf;
    socket_counts->index = (u8)index;
    socket_counts->pid = entry->pid;
    socket_counts->socket_count = entry->socket_count;
    socket_counts->socket_types = entry->socket_types;
    socket_counts->socket_states = entry->socket_states;
    *len = sizeof(msg_linux_process_socket_counts_t);
  } break;
  case SEND_DONE: break;
  case SEND_SOCKET_QUEUES_0:
  case SEND_SOCKET_QUEUES_1:
  case SEND_SOCKET_QUEUES_2:
  case SEND_SOCKET_QUEUES_3:
  case SEND_SOCKET_QUEUES_4:
  case SEND_SOCKET_QUEUES_5:
  case SEND_SOCKET_QUEUES_6:
  case SEND_SOCKET_QUEUES_7:
  case SEND_SOCKET_QUEUES_8:
  case SEND_SOCKET_QUEUES_9:
  default: PK_LOG_ANNO(LOG_ERR, "Invalid state value: %d", state->send_state);
  }

  return true;
}

static void *init_resource_query()
{
  u16 pid_count = 0;

  resq_state_t *state = calloc(1, sizeof(resq_state_t));
  resq_read_property_t read_prop = {.id = QUERY_SYS_PROP_PID_COUNT, .type = RESQ_PROP_U16};

  if (!resq_read_property(QUERY_SYS_STATE_NAME, &read_prop)) {
    PK_LOG_ANNO(LOG_ERR,
                "failed to read 'pid count' property from '%s' module",
                QUERY_SYS_STATE_NAME);
    goto error;
  }

  pid_count = read_prop.property.u16;
  PK_LOG_ANNO(LOG_DEBUG,
              "'%s' module reported %d processes on the system",
              QUERY_SYS_STATE_NAME,
              pid_count);

  if (pid_count > MAX_PROCESS_COUNT) {
    PK_LOG_ANNO(LOG_ERR,
                "detected more than %d processes on the system: %d",
                MAX_PROCESS_COUNT,
                pid_count);
    goto error;
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
  .priority = RESQ_PRIORITY_2,
  .init = init_resource_query,
  .describe = describe_query,
  .read_property = NULL,
  .run_query = run_resource_query,
  .prepare_sbp = query_sockets_prepare,
  .teardown = teardown_resource_query,
};

static __attribute__((constructor)) void register_cpu_query()
{
  resq_register(&query_descriptor);
}
