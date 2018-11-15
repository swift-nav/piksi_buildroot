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

//#define DEBUG_QUERY_FD_TAB

#define ITEM_COUNT 10
#define MAX_PROCESS_COUNT 2048
#define MAX_FILE_COUNT 4096
#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255
#define LSOF_CMD_BUF_LENGTH 1024 * 128

typedef enum {
  SEND_FD_COUNTS_0,
  SEND_FD_COUNTS_1,
  SEND_FD_COUNTS_2,
  SEND_FD_COUNTS_3,
  SEND_FD_COUNTS_4,
  SEND_FD_COUNTS_5,
  SEND_FD_COUNTS_6,
  SEND_FD_COUNTS_7,
  SEND_FD_COUNTS_8,
  SEND_FD_COUNTS_9,
  SEND_FD_SUMMARY,
  SEND_DONE,
} send_state_t;

typedef struct {
  char id_str[16];
  u16 pid;
  u16 fd_count;
} pid_entry_t;

typedef struct {
  char file_str[128];
  u16 fd_count;
} file_entry_t;

MAKE_TABLE_WRAPPER(pid, pid_entry_t);
MAKE_TABLE_WRAPPER(file, file_entry_t);

typedef struct {
  send_state_t send_state;
  u32 total_fd_count;
  table_t *pid_table;
  table_t *file_table;
  const char *top10_pid_counts[ITEM_COUNT];
  const char *top10_file_counts[ITEM_COUNT];
} resq_state_t;

typedef struct id_to_count {
  u16 count;
  const char *id;
} id_to_count_t;

enum {
  FD_STATE_ID_STR = 0,
  FD_STATE_EXE_FILE_STR,
  FD_STATE_FD_STR,
  FD_STATE_DONE,
  FD_STATE_COUNT,
};

static bool is_real_file_path(const char *path)
{
  if (strncmp(path, "pipe", 4) == 0 || strncmp(path, "socket", 6) == 0)
    return false;
  else
    return true;
}

static bool process_fd_entry(resq_state_t *state, const char *file_str, const char *id_str)
{
  pid_entry_t *entry = pid_table_get(state->pid_table, id_str);
  state->total_fd_count++;
  if (entry == NULL) {
    pid_entry_t new_entry = {0};
    int s = snprintf(new_entry.id_str, sizeof(entry->id_str), "%s", id_str);
    if (s < 0 || (size_t)s >= sizeof(entry->id_str)) {
      PK_LOG_ANNO(LOG_WARNING, "file descriptor query: failed to copy pid string");
      return false;
    }
    unsigned long pid = 0;
    if (!strtoul_all(10, id_str, &pid)) {
      PK_LOG_ANNO(LOG_WARNING,
                  "file descriptor query: failed to convert pid str to number: %s",
                  strerror(errno));
      return false;
    }
    new_entry.pid = (u16)pid;
    entry = calloc(1, sizeof(pid_entry_t));
    *entry = new_entry;
    bool added = pid_table_put(state->pid_table, id_str, entry);
    if (!added) {
      PK_LOG_ANNO(LOG_ERR | LOG_SBP, "failed to add entry for pid '%s'", id_str);
      free(entry);
      return false;
    }
  }
  entry->fd_count++;
  if (is_real_file_path(file_str)) {
    file_entry_t *file_entry = file_table_get(state->file_table, file_str);
    if (file_entry == NULL) {
      file_entry_t new_entry = {0};
      int s = snprintf(new_entry.file_str, sizeof(new_entry.file_str), "%s", file_str);
      if (s < 0 || (size_t)s >= sizeof(file_entry->file_str)) {
        PK_LOG_ANNO(LOG_WARNING,
                    "file descriptor query: failed to copy file string '%s'",
                    file_str);
        return false;
      }
      file_entry = calloc(1, sizeof(file_entry_t));
      *file_entry = new_entry;
      bool added = file_table_put(state->file_table, file_str, file_entry);
      if (!added) {
        PK_LOG_ANNO(LOG_WARNING,
                    "file descriptor query: failed to add entry for file string '%s'",
                    file_str);
        free(file_entry);
        return false;
      }
    }
    file_entry->fd_count++;
  }
  return true;
}

static bool parse_fd_line(resq_state_t *state, const char *line)
{
  char id_str[16] = {0};
  char exe_str[32] = {0};
  char file_str[64] = {0};
  line_spec_t line_specs[FD_STATE_COUNT] = {[FD_STATE_ID_STR] =
                                              (line_spec_t){
                                                .type = FT_STR,
                                                .dst.str = id_str,
                                                .buflen = sizeof(id_str),
                                                .desc = "pid",
                                                .next = FD_STATE_EXE_FILE_STR,
                                              },
                                            [FD_STATE_EXE_FILE_STR] =
                                              (line_spec_t){
                                                .type = FT_STR,
                                                .dst.str = exe_str,
                                                .buflen = sizeof(exe_str),
                                                .desc = "executable",
                                                .next = FD_STATE_FD_STR,
                                              },
                                            [FD_STATE_FD_STR] =
                                              (line_spec_t){
                                                .type = FT_STR,
                                                .dst.str = file_str,
                                                .buflen = sizeof(file_str),
                                                .desc = "file descriptor",
                                                .next = FD_STATE_DONE,
                                              },
                                            [FD_STATE_DONE] = (line_spec_t){
                                              .desc = "done",
                                            }};

  if (!parse_tab_line(line, FD_STATE_ID_STR, FD_STATE_DONE, line_specs, "\t")) {
#ifdef DEBUG_QUERY_FD_TAB
    PK_LOG_ANNO(LOG_ERR, "file descriptor query: parse_tab_line failed ");
#endif
    return false;
  }
  return process_fd_entry(state, file_str, id_str);
}

static void destroy_table_entry(table_t *table, void **entry)
{
  (void)table;
  free(*entry);
  *entry = NULL;
}

static int compare_counts(const void *ptc1, const void *ptc2)
{
  const id_to_count_t *id_to_count1 = ptc1;
  const id_to_count_t *id_to_count2 = ptc2;
  return -((int)id_to_count1->count - (int)id_to_count2->count);
}

static void run_resource_query(void *context)
{
  resq_state_t *state = context;
  if (state->pid_table != NULL) {
    table_destroy(&state->pid_table);
  }
  if (state->file_table != NULL) {
    table_destroy(&state->file_table);
  }
  char leftover_buffer[4096] = {0};
  leftover_t NESTED_AXX(leftover) = {.buf = leftover_buffer};
  state->send_state = SEND_FD_COUNTS_0;
  state->pid_table = table_create(MAX_PROCESS_COUNT, destroy_table_entry);
  state->file_table = table_create(MAX_FILE_COUNT, destroy_table_entry);
  state->total_fd_count = 0;
  if (state->pid_table == NULL) {
    PK_LOG_ANNO(LOG_ERR, "file descriptor query: pid table creation failed");
    return;
  }
  if (state->file_table == NULL) {
    PK_LOG_ANNO(LOG_ERR, "file descriptor query: file table creation failed");
    return;
  }
  const char *argv[] = {"sudo", "/usr/bin/lsof", NULL};
  char buf[LSOF_CMD_BUF_LENGTH] = {0};
  int rc = run_with_stdin_file(NULL, "sudo", argv, buf, sizeof(buf));
  if (rc != 0) {
#ifdef DEBUG_QUERY_FD_TAB
    PK_LOG_ANNO(LOG_ERR,
                "file descriptor query: error running 'lsof' command: %s",
                strerror(errno));
#endif
    return;
  }
  line_fn_t line_func = NESTED_FN(bool, (const char *line), {
    if (!parse_fd_line(state, line)) {
#ifdef DEBUG_QUERY_FD_TAB
      PK_LOG_ANNO(LOG_ERR, "file descriptor query: parse_fd_line failed ");
#endif
      return false;
    }
    return true;
  });
  ssize_t consumed = foreach_line(buf, &leftover, line_func);
  if (consumed < 0) {
    PK_LOG_ANNO(LOG_WARNING, "there was an error parsing the 'ss' output");
    return;
  }

  size_t pid_count = table_count(state->pid_table);
  id_to_count_t *NESTED_AXX(pid_to_count) = alloca(pid_count * sizeof(id_to_count_t));

  table_foreach_key(state->pid_table,
                    NULL,
                    NESTED_FN(bool,
                              (table_t * table, const char *key, size_t index, void *context1),
                              {
                                (void)context1;
                                pid_entry_t *entry = pid_table_get(table, key);
                                pid_to_count[index].id = key;
                                pid_to_count[index].count = entry->fd_count;
                                return true;
                              }));

  qsort(pid_to_count, pid_count, sizeof(id_to_count_t), compare_counts);
  size_t file_count = table_count(state->file_table);
  id_to_count_t *NESTED_AXX(file_to_count) = alloca(file_count * sizeof(id_to_count_t));

  table_foreach_key(state->file_table,
                    NULL,
                    NESTED_FN(bool,
                              (table_t * table, const char *key, size_t index, void *context1),
                              {
                                (void)context1;
                                file_entry_t *entry = file_table_get(table, key);
                                file_to_count[index].id = key;
                                file_to_count[index].count = entry->fd_count;
                                return true;
                              }));

  qsort(file_to_count, file_count, sizeof(id_to_count_t), compare_counts);
  for (size_t i = 0; i < ITEM_COUNT && i < pid_count; i++) {
    state->top10_pid_counts[i] = pid_to_count[i].id;
#ifdef DEBUG_QUERY_FD_TAB
    PK_LOG_ANNO(LOG_DEBUG, "TOP 10 FD PID : %s : %d", pid_to_count[i].id, pid_to_count[i].count);
#endif
  }
  for (size_t i = 0; i < ITEM_COUNT && i < file_count; i++) {
    state->top10_file_counts[i] = file_to_count[i].id;
#ifdef DEBUG_QUERY_FD_TAB
    PK_LOG_ANNO(LOG_DEBUG, "TOP 10 FD FILE : %s : %d", file_to_count[i].id, file_to_count[i].count);
#endif
  }
}

static bool query_fd_prepare(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context)
{
  resq_state_t *state = context;
  if (state->send_state == SEND_DONE) return false;
  assert(0 == SEND_FD_COUNTS_0);
  size_t index = (size_t)state->send_state;
  switch (state->send_state) {
  case SEND_FD_COUNTS_0:
  case SEND_FD_COUNTS_1:
  case SEND_FD_COUNTS_2:
  case SEND_FD_COUNTS_3:
  case SEND_FD_COUNTS_4:
  case SEND_FD_COUNTS_5:
  case SEND_FD_COUNTS_6:
  case SEND_FD_COUNTS_7:
  case SEND_FD_COUNTS_8:
  case SEND_FD_COUNTS_9: {
    pid_entry_t *entry = pid_table_get(state->pid_table, state->top10_pid_counts[index]);
    *msg_type = SBP_MSG_LINUX_PROCESS_FD_COUNT;
    msg_linux_process_fd_count_t *fd_counts = (msg_linux_process_fd_count_t *)sbp_buf;
    fd_counts->index = (u8)index;
    fd_counts->pid = entry->pid;
    fd_counts->fd_count = entry->fd_count;
#ifdef DEBUG_QUERY_FD_TAB
    PK_LOG_ANNO(LOG_DEBUG,
                "index : %d pid: %d : fd_count : %d\n",
                fd_counts->index,
                fd_counts->pid,
                fd_counts->fd_count);
#endif
    *len = sizeof(msg_linux_process_fd_count_t);
  } break;
  case SEND_FD_SUMMARY:
    *msg_type = SBP_MSG_LINUX_PROCESS_FD_SUMMARY;
    msg_linux_process_fd_summary_t *fd_summary = (msg_linux_process_fd_summary_t *)sbp_buf;
    fd_summary->sys_fd_count = state->total_fd_count;
    int nBytes = 0;
    for (int i = 0; i < ITEM_COUNT; i++) {
      unsigned int buf_space =
        (unsigned int)(SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(msg_linux_process_fd_summary_t)
                       - (unsigned int)nBytes);
      file_entry_t *file_entry = file_table_get(state->file_table, state->top10_file_counts[i]);
      nBytes += snprintf(&fd_summary->most_opened[nBytes],
                         buf_space,
                         "%d%c%s",
                         file_entry->fd_count,
                         '\0',
                         file_entry->file_str);
#ifdef DEBUG_QUERY_FD_TAB
      PK_LOG_ANNO(LOG_DEBUG,
                  "Summary:%d : %d %s bytes",
                  nBytes,
                  file_entry->fd_count,
                  file_entry->file_str);
#endif
      if ((SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(msg_linux_process_fd_summary_t) - 1)
          < (unsigned int)nBytes) {
        nBytes = (SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(msg_linux_process_fd_summary_t) - 1);
        break;
      }
    }
    fd_summary->most_opened[nBytes] = '\0'; // set two NULL to indicate termination
    *len = (u8)(sizeof(msg_linux_process_fd_summary_t) + (unsigned)nBytes + 1);
    break;
  case SEND_DONE:
  default: {
    PK_LOG_ANNO(LOG_ERR, "file descriptor query: invalid state value: %d", state->send_state);
    return false;
  }
  }
  state->send_state++;
  return true;
}

static void *init_resource_query()
{
  u16 pid_count = 0;
  resq_state_t *state = calloc(1, sizeof(resq_state_t));
  resq_read_property_t read_prop = {.id = QUERY_SYS_PROP_PID_COUNT, .type = RESQ_PROP_U16};
  if (!resq_read_property(QUERY_SYS_STATE_NAME, &read_prop)) {
    PK_LOG_ANNO(LOG_WARNING,
                "failed to read 'pid count' property from '%s' module",
                QUERY_SYS_STATE_NAME);
    goto error;
  }
  pid_count = read_prop.property.u16;
#ifdef DEBUG_QUERY_FD_TAB
  PK_LOG_ANNO(LOG_DEBUG,
              "'%s' module reported %d processes on the system",
              QUERY_SYS_STATE_NAME,
              pid_count);
#endif
  if (pid_count > MAX_PROCESS_COUNT) {
    PK_LOG_ANNO(LOG_ERR | LOG_SBP,
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
  return "process file descriptor";
}

static void teardown_resource_query(void **context)
{
  if (context == NULL || *context == NULL) return;
  resq_state_t *state = *context;
  if (state->pid_table != NULL) {
    table_destroy(&state->pid_table);
  }
  if (state->file_table != NULL) {
    table_destroy(&state->file_table);
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
  .prepare_sbp = query_fd_prepare,
  .teardown = teardown_resource_query,
};

static __attribute__((constructor)) void register_cpu_query()
{
  resq_register(&query_descriptor);
}
