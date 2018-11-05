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
#include <dirent.h>
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
  SEND_DONE,
} send_state_t;

typedef struct {
  char pid_str[16];
  u16 pid;
  u16 fd_count;
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

enum {
  FD_STATE_PERMISSION_STR = 0,
  FD_STATE_PERMISSION_INT,
  FD_STATE_OWNER_STR,
  FD_STATE_ROLE_STR,
  FD_STATE_ROLE_INT,
  FD_STATE_MONTH,
  FD_STATE_DATE,
  FD_STATE_TIME,
  FD_STATE_INDEX,
  FD_STATE_ARROW,
  FD_STATE_FILE_DESCRIPTOR,
  FD_STATE_DONE,
  FD_STATE_COUNT,
};

static bool is_real_file_path(const char *path)
{
  if (strncmp(path, "/tmp", 4) == 0 || strncmp(path, "'socket:", 8) == 0)
    return false;
  else
    return true;
}

static bool process_fd_entry(resq_state_t *state, const char *file_str, const char *pid_str)
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
      return false;
    }

    unsigned long pid = 0;
    if (!strtoul_all(10, pid_str, &pid)) {
      PK_LOG_ANNO(LOG_ERR, "failed to convert pid str to number: %s", strerror(errno));
      return false;
    }

    new_entry.pid = (u16)pid;

    entry = calloc(1, sizeof(pid_entry_t));
    *entry = new_entry;

    bool added = pid_table_put(state->pid_table, pid_str, entry);

    if (!added) {
      PK_LOG_ANNO(LOG_ERR, "failed add entry for pid '%s'", pid_str);
      free(entry);
      return false;
    }

  } else {
#ifdef DEBUG_QUERY_SOCKETS_TAB
    PK_LOG_ANNO(LOG_DEBUG, "found existing entry for %s", pid_str);
#endif
  }
  if (is_real_file_path(file_str)) entry->fd_count++;
  return true;
  /* TODO: need to get command line from /proc and copy into pid_state_t */
}

static bool parse_fd_line(resq_state_t *state, const char *line, const char *pid_str)
{
  char permission_str[10] = {0};
  char owner_str[32] = {0};
  char role_str[32] = {0};
  char month_str[8] = {0};
  char time_str[8] = {0};
  char arrow_str[8] = {0};
  char file_str[32] = {0};
  u32 permission_uint = 0;
  u32 role_int = 0;
  u32 date = 0;
  u32 index = 0;


  line_spec_t line_specs[FD_STATE_COUNT] = {[FD_STATE_PERMISSION_STR] =
                                              (line_spec_t){
                                                .type = FT_STR,
                                                .dst.str = permission_str,
                                                .buflen = sizeof(permission_str),
                                                .desc = "permission",
                                                .next = FD_STATE_PERMISSION_INT,
                                              },
                                            [FD_STATE_PERMISSION_INT] =
                                              (line_spec_t){
                                                .type = FT_U32,
                                                .dst.u32 = &permission_uint,
                                                .desc = "permission(int)",
                                                .next = FD_STATE_OWNER_STR,
                                              },
                                            [FD_STATE_OWNER_STR] =
                                              (line_spec_t){
                                                .type = FT_STR,
                                                .dst.str = owner_str,
                                                .buflen = sizeof(owner_str),
                                                .desc = "owner",
                                                .next = FD_STATE_ROLE_STR,
                                              },
                                            [FD_STATE_ROLE_STR] =
                                              (line_spec_t){
                                                .type = FT_STR,
                                                .dst.str = role_str,
                                                .buflen = sizeof(role_str),
                                                .desc = "role",
                                                .next = FD_STATE_ROLE_INT,
                                              },
                                            [FD_STATE_ROLE_INT] =
                                              (line_spec_t){
                                                .type = FT_U32,
                                                .dst.u32 = &role_int,
                                                .desc = "role(int)",
                                                .next = FD_STATE_MONTH,
                                              },
                                            [FD_STATE_MONTH] =
                                              (line_spec_t){
                                                .type = FT_STR,
                                                .dst.str = month_str,
                                                .buflen = sizeof(month_str),
                                                .desc = "month",
                                                .next = FD_STATE_DATE,
                                              },
                                            [FD_STATE_DATE] =
                                              (line_spec_t){
                                                .type = FT_U32,
                                                .dst.u32 = &date,
                                                .desc = "date",
                                                .next = FD_STATE_TIME,
                                              },
                                            [FD_STATE_TIME] =
                                              (line_spec_t){
                                                .type = FT_STR,
                                                .dst.str = time_str,
                                                .buflen = sizeof(time_str),
                                                .desc = "time",
                                                .next = FD_STATE_INDEX,
                                              },
                                            [FD_STATE_INDEX] =
                                              (line_spec_t){
                                                .type = FT_U32,
                                                .dst.u32 = &index,
                                                .desc = "index",
                                                .next = FD_STATE_ARROW,
                                              },
                                            [FD_STATE_ARROW] =
                                              (line_spec_t){
                                                .type = FT_STR,
                                                .dst.str = arrow_str,
                                                .buflen = sizeof(arrow_str),
                                                .desc = "arrow",
                                                .next = FD_STATE_FILE_DESCRIPTOR,
                                              },
                                            [FD_STATE_FILE_DESCRIPTOR] =
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

  if (!parse_tab_line(line, FD_STATE_PERMISSION_STR, FD_STATE_DONE, line_specs, " ")) {
    return false;
  }

  return process_fd_entry(state, file_str, pid_str);
}

static void destroy_table_entry(table_t *table, void **entry)
{
  (void)table;

  pid_entry_t *pid_entry = *entry;
  free(pid_entry);

  *entry = NULL;
}

static bool is_valid_pid(const char *str)
{
  unsigned int length = strlen(str);
  for (unsigned int i = 0; i < length; i++) {
    if (!isdigit(str[i])) return false;
  }
  return true;
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

  state->send_state = SEND_FD_COUNTS_0;
  state->pid_table = table_create(MAX_PROCESS_COUNT, destroy_table_entry);

  if (state->pid_table == NULL) {
    PK_LOG_ANNO(LOG_ERR, "table creation failed");
    return;
  }

  DIR *dir;
  struct dirent *ent;

  dir = opendir("/proc");


  while ((ent = readdir(dir)) != NULL) {
    if (is_valid_pid(ent->d_name)) {
      line_fn_t line_func = NESTED_FN(bool, (const char *line), {
#ifdef DEBUG_QUERY_SOCKETS_SS_OUTPUT
        PK_LOG_ANNO(LOG_DEBUG, "fd query command: '%s'", line);
#endif
        if (strstr(line, "total") == line) {
          PK_LOG_ANNO(LOG_DEBUG, "Skipping header line: '%s'", line);
          return true;
        } else {
          if (!parse_fd_line(state, line, ent->d_name)) {
            PK_LOG_ANNO(LOG_WARNING, "failed to parse uds 'ss' line: %s", line);
          }
        }
        return true;
      });

      buffer_fn_t buffer_fn = NESTED_FN(ssize_t, (char *buffer, size_t length, void *ctx), {

        (void)ctx;
        /* foreach_line requires a null terminated string */
        buffer[length] = '\0';

        ssize_t consumed = foreach_line(buffer, &leftover, line_func);
        if (consumed < 0) {
          PK_LOG_ANNO(LOG_WARNING, "there was an error parsing the 'ss' output");
          return -1;
        }

        if (leftover.line_count == 0 && ++no_line_count >= 2) {
          PK_LOG_ANNO(LOG_WARNING, "command output had no newlines");
          return -1;
        }

        no_line_count = 0;

        return (ssize_t)length;
      });
      char fdpath[32];
      sprintf(&fdpath[0], "//proc//%s//fd", ent->d_name);
      run_command_t pre_run_config =
        {.input = NULL,
         .context = "cd",
         .argv = (const char *[]){"cd", fdpath, NULL},
         .buffer = output_buffer,
         .length = sizeof(leftover_buffer) - 1,
         .func = NULL};
      PK_LOG_ANNO(LOG_ERR,
                  "Trying to execute : cd %s",
				  fdpath);

      if (run_command(&pre_run_config) != 0) {
        PK_LOG_ANNO(LOG_ERR,
                    "file descriptor query failed: %s (errno: %d)",
                    strerror(errno),
                    errno);
        return;
      }
      run_command_t run_config =
        {.input = NULL,
         .context = NULL,
         .argv = (const char *[]){"ls", "-l", NULL},
         .buffer = output_buffer,
         .length = sizeof(leftover_buffer) - 1,
         .func = buffer_fn};
      PK_LOG_ANNO(LOG_ERR,
                  "Trying to execute : ls -l",
				  fdpath);

      if (run_command(&run_config) != 0) {
        PK_LOG_ANNO(LOG_ERR,
                    "file descriptor query failed: %s (errno: %d)",
                    strerror(errno),
                    errno);
        return;
      }
    }
  }

  closedir(dir);


  size_t pid_count = table_count(state->pid_table);

  pid_to_count_t *NESTED_AXX(pid_to_count) = alloca(pid_count * sizeof(pid_to_count_t));

  table_foreach_key(state->pid_table,
                    NULL,
                    NESTED_FN(bool,
                              (table_t * table, const char *key, size_t index, void *context1),
                              {

                                (void)context1;

                                pid_entry_t *entry = pid_table_get(table, key);

                                pid_to_count[index].pid = key;
                                pid_to_count[index].count = entry->fd_count;

                                return true;
                              }));

  qsort(pid_to_count, pid_count, sizeof(pid_to_count_t), compare_counts);


  for (size_t i = 0; i < ITEM_COUNT && i < pid_count; i++) {
    state->top10_counts[i] = pid_to_count[i].pid;
  }
}

static bool query_fd_prepare(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context)
{
  resq_state_t *state = context;
  size_t index = 0;

  switch (state->send_state) {
  case SEND_FD_COUNTS_0:
    index = 0;
    state->send_state = SEND_FD_COUNTS_1;
    break;
  case SEND_FD_COUNTS_1:
    index = 1;
    state->send_state = SEND_FD_COUNTS_2;
    break;
  case SEND_FD_COUNTS_2:
    index = 2;
    state->send_state = SEND_FD_COUNTS_3;
    break;
  case SEND_FD_COUNTS_3:
    index = 3;
    state->send_state = SEND_FD_COUNTS_4;
    break;
  case SEND_FD_COUNTS_4:
    index = 4;
    state->send_state = SEND_FD_COUNTS_5;
    break;
  case SEND_FD_COUNTS_5:
    index = 5;
    state->send_state = SEND_FD_COUNTS_6;
    break;
  case SEND_FD_COUNTS_6:
    index = 6;
    state->send_state = SEND_FD_COUNTS_7;
    break;
  case SEND_FD_COUNTS_7:
    index = 7;
    state->send_state = SEND_FD_COUNTS_8;
    break;
  case SEND_FD_COUNTS_8:
    index = 8;
    state->send_state = SEND_FD_COUNTS_9;
    break;
  case SEND_FD_COUNTS_9:
    index = 9;
    state->send_state = SEND_DONE;
    break;
  case SEND_DONE:
  default: PK_LOG_ANNO(LOG_ERR, "Invalid state value: %d", state->send_state);
  }

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
    pid_entry_t *entry = pid_table_get(state->pid_table, state->top10_counts[index]);
    *msg_type = SBP_MSG_LINUX_PROCESS_FD_COUNT;
    msg_linux_process_fd_count_t *fd_counts = (msg_linux_process_fd_count_t *)sbp_buf;
    fd_counts->index = (u8)index;
    fd_counts->pid = entry->pid;
    fd_counts->fd_count = entry->fd_count;
    *len = sizeof(msg_linux_process_fd_count_t);
  } break;
  case SEND_DONE:
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
  return "process file descriptor";
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
  .priority = RESQ_PRIORIRTY_2,
  .init = init_resource_query,
  .describe = describe_query,
  .read_property = NULL,
  .run_query = run_resource_query,
  .prepare_sbp = query_fd_prepare,
  .teardown = teardown_resource_query,
};

static __attribute__((constructor)) void register_cpu_query()
{
	(void)query_descriptor;
//  resq_register(&query_descriptor);
}
