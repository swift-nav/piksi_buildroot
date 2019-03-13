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

#include <limits.h>
#include <search.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <libsbp/linux.h>

#include <libpiksi/cast_check.h>
#include <libpiksi/kmin.h>
#include <libpiksi/logging.h>
#include <libpiksi/runit.h>
#include <libpiksi/table.h>
#include <libpiksi/util.h>

#include "query_sys_state.h"
#include "resource_monitor.h"
#include "resource_query.h"
#include "resmon_common.h"

#define ITEM_COUNT 10
#define MAX_PROCESS_COUNT 2048
#define MAX_FILE_COUNT (1024 * 16)
#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255
#define LSOF_CMD_BUF_LENGTH (1024 * 4)
#define COMMAND_LINE_MAX (256u)

/* clang-format off */
/* #define DEBUG_QUERY_FD_TAB */
#ifdef DEBUG_QUERY_FD_TAB
#  define QPF_DEBUG_LOG(ThePattern, ...) \
     PK_LOG_ANNO(LOG_DEBUG, ThePattern, ##__VA_ARGS__)
#else
#  define QPF_DEBUG_LOG(...)
#endif
/* clang-format on */

#define WHEN(WhenExpr) WhenExpr

#define AND_REPORT(TheMsg) TheMsg

#define GOTO_FAIL(Expr, Msg)     \
  do {                           \
    if ((Expr)) {                \
      PK_LOG_ANNO(LOG_ERR, Msg); \
      goto fail;                 \
    }                            \
  } while (0)


typedef struct {
  bool put_success;
  kmin_t *max_fds;
} populate_ctx_t;

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
  SEND_ERROR
} send_state_t;

typedef struct {
  char id_str[16];
  u16 pid;
  u16 fd_count;
  char cmdline[COMMAND_LINE_MAX];
} pid_entry_t;

typedef struct {
  char file_str[128];
  u16 fd_count;
} file_entry_t;

MAKE_TABLE_WRAPPER(pid, pid_entry_t);
MAKE_TABLE_WRAPPER(file, file_entry_t);

typedef struct {
  send_state_t send_state;
  u8 error_code;
  u32 total_fd_count;
  table_t *pid_table;
  table_t *file_table;
  const char *top10_pid_counts[ITEM_COUNT];
  const char *top10_file_counts[ITEM_COUNT];
} resq_state_t;

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

static bool get_proc_cmdline(const char *id_str, char *buf, size_t buf_len)
{
  const char *the_command[] = {"sudo", "/usr/bin/get_proc_cmdline", id_str, NULL};

  buffer_fn_t buffer_fn = NESTED_FN(ssize_t, (char *buffer, size_t length, void *ctx), {
    (void)ctx;
    for (unsigned int i = 0; i < length; i++) {
      if (buffer[i] == '\0') {
        buffer[i] = ' ';
      }
    }
    return (ssize_t)length;
  });

  run_command_t run_config = {.argv = the_command,
                              .buffer = buf,
                              .length = buf_len,
                              .func = buffer_fn};

  if (run_command(&run_config) != 0) {
    return false;
  }

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
    new_entry.cmdline[0] = '\0';
    entry = calloc(1, sizeof(pid_entry_t));
    *entry = new_entry;
    bool added = pid_table_put(state->pid_table, id_str, entry);
    if (!added) {
      PK_LOG_ANNO(LOG_ERR, "failed to add entry for pid '%s'", id_str);
      free(entry);
      return false;
    }
  }
  if (entry != NULL) {
    entry->fd_count++;
  }
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
        PK_LOG_ANNO(LOG_ERR,
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
  char exe_str[NAME_MAX] = {0};
  char file_str[PATH_MAX] = {0};
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
    PK_LOG_ANNO(LOG_WARNING, "file descriptor query: parse_tab_line failed :%s\n", line);
    return false;
  }
  return process_fd_entry(state, file_str, id_str);
}

static void populate_maximums(table_t *the_table,
                              char const *top10[],
                              table_foreach_fn_t foreach_key_fn)
{
  for (size_t i = 0; i < ITEM_COUNT; i++) {
    top10[i] = "";
  }

  size_t table_count_ = table_count(the_table);
  kmin_t *max_fds = kmin_create(table_count_);

  GOTO_FAIL(WHEN(max_fds == NULL), AND_REPORT("kmin_create failed"));

  /* Get maximums instead of minimums */
  kmin_invert(max_fds, true);

  populate_ctx_t ctx = {.put_success = true, .max_fds = max_fds};

  table_foreach_key(the_table, &ctx, foreach_key_fn);
  GOTO_FAIL(WHEN(!ctx.put_success), AND_REPORT("kmin_put failed"));

  bool compact = kmin_compact(max_fds);
  GOTO_FAIL(WHEN(!compact), AND_REPORT("kmin_compact failed"));

  ssize_t kmin_count = kmin_find(max_fds, 0, ITEM_COUNT);
  GOTO_FAIL(WHEN(kmin_count < 0), AND_REPORT("kmin_find failed"));

  for (size_t i = 0; i < ssizet_to_sizet(kmin_count); i++) {
    top10[i] = kmin_ident_at(max_fds, i);
  }

fail:
  kmin_destroy(&max_fds);
}

static void populate_max_by_pid(resq_state_t *state)
{
  table_foreach_fn_t foreach_key_fn =
    NESTED_FN(bool, (table_t * table, const char *key, size_t index, void *p), {
      populate_ctx_t *ctx = p;

      pid_entry_t *entry = pid_table_get(table, key);
      ctx->put_success = kmin_put(ctx->max_fds, index, entry->fd_count, key);

      return ctx->put_success;
    });

  populate_maximums(state->pid_table, state->top10_pid_counts, foreach_key_fn);
}

static void populate_max_by_file(resq_state_t *state)
{
  table_foreach_fn_t foreach_key_fn =
    NESTED_FN(bool, (table_t * table, const char *key, size_t index, void *p), {
      populate_ctx_t *ctx = p;

      file_entry_t *entry = file_table_get(table, key);
      if (strncmp(key, "/", 1) != 0) return true;

      ctx->put_success = kmin_put(ctx->max_fds, index, entry->fd_count, key);

      return ctx->put_success;
    });

  populate_maximums(state->file_table, state->top10_file_counts, foreach_key_fn);
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

  char leftover_buffer[LSOF_CMD_BUF_LENGTH] = {0};
  leftover_t NESTED_AXX(leftover) = {.buf = leftover_buffer, .size = 0};

  size_t NESTED_AXX(no_line_count) = 0;

  state->error_code = 0;
  state->send_state = SEND_FD_COUNTS_0;
  state->pid_table = table_create(MAX_PROCESS_COUNT);
  state->file_table = table_create(MAX_FILE_COUNT);
  state->total_fd_count = 0;

  if (state->pid_table == NULL) {
    PK_LOG_ANNO(LOG_ERR, "file descriptor query: pid table creation failed");
    state->error_code = 1;
    state->send_state = SEND_ERROR;
    return;
  }

  if (state->file_table == NULL) {
    PK_LOG_ANNO(LOG_ERR, "file descriptor query: file table creation failed");
    state->error_code = 2;
    state->send_state = SEND_ERROR;
    return;
  }

  line_fn_t line_func = NESTED_FN(bool, (const char *line), {
    if (!parse_fd_line(state, line)) {
      PK_LOG_ANNO(LOG_WARNING, "file descriptor query: parse_fd_line failed ");
      return false;
    }
    return true;
  });

  buffer_fn_t buffer_fn = NESTED_FN(ssize_t, (char *buffer, size_t length, void *ctx), {

    (void)ctx;

    /* foreach_line requires a null terminated string */
    buffer[length] = '\0';

    QPF_DEBUG_LOG("length: %d, buffer: <<%s>>", length, buffer);

    ssize_t consumed = foreach_line(buffer, &leftover, line_func);
    if (consumed < 0) {
      PK_LOG_ANNO(LOG_WARNING, "there was an error parsing the 'lsof' output");
      return -1;
    }

    if (leftover.line_count == 0 && ++no_line_count >= 2) {
      PK_LOG_ANNO(LOG_WARNING, "command output had no newlines");
      return -1;
    }

    if (leftover.size > 0) {
      QPF_DEBUG_LOG("leftover: '%s'", leftover.buf);
    }

    no_line_count = 0;
    return (ssize_t)length;
  });

  char buf[LSOF_CMD_BUF_LENGTH] = {0};
  run_command_t run_config = {.input = NULL,
                              .context = NULL,
                              .argv = (const char *[]){"sudo", "/usr/bin/lsof", NULL},
                              .buffer = buf,
                              .length = sizeof(leftover_buffer) - 1,
                              .func = buffer_fn};

  if (run_command(&run_config) != 0) {
    PK_LOG_ANNO(LOG_ERR,
                "file descriptor query: error running 'lsof' command: %s",
                strerror(errno));
    state->error_code = 3;
    state->send_state = SEND_ERROR;
    return;
  }

  populate_max_by_pid(state);

  populate_max_by_file(state);
}

static bool query_fd_prepare(u16 *msg_type, u8 *len, u8 *sbp_buf, void *context)
{
  resq_state_t *state = context;

  if (state->send_state == SEND_DONE) return false;

  if (table_count(state->pid_table) < ITEM_COUNT || table_count(state->file_table) < ITEM_COUNT) {
    state->error_code = 4;
    state->send_state = SEND_ERROR;
  }

  static_assert(0 == SEND_FD_COUNTS_0, "SEND_FD_COUNTS_0 value should be 0");

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

    const char *pid_str = state->top10_pid_counts[index];

    if (pid_str[0] == '\0') {
      return false;
    }

    pid_entry_t *entry = pid_table_get(state->pid_table, pid_str);

    *msg_type = SBP_MSG_LINUX_PROCESS_FD_COUNT;
    msg_linux_process_fd_count_t *fd_counts = (msg_linux_process_fd_count_t *)sbp_buf;

    fd_counts->index = (u8)index;
    fd_counts->pid = entry->pid;
    fd_counts->fd_count = entry->fd_count;

    if (entry->cmdline[0] == '\0') {
      get_proc_cmdline(pid_str, entry->cmdline, sizeof(entry->cmdline));
    }

    size_t cmdline_len = strlen(entry->cmdline);
    if (cmdline_len + sizeof(msg_linux_process_fd_count_t) > SBP_FRAMING_MAX_PAYLOAD_SIZE)
      cmdline_len = SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(msg_linux_process_fd_count_t);

    strncpy(fd_counts->cmdline, entry->cmdline, cmdline_len);

    QPF_DEBUG_LOG("index : %d pid: %d : fd_count : %d cmd line : %s",
                  fd_counts->index,
                  fd_counts->pid,
                  fd_counts->fd_count,
                  fd_counts->cmdline);

    *len = (u8)(sizeof(msg_linux_process_fd_count_t) + cmdline_len);

  } break;

  case SEND_FD_SUMMARY: {

    *msg_type = SBP_MSG_LINUX_PROCESS_FD_SUMMARY;

    msg_linux_process_fd_summary_t *fd_summary = (msg_linux_process_fd_summary_t *)sbp_buf;
    fd_summary->sys_fd_count = state->total_fd_count;

    const size_t fd_summary_max_size =
      SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(msg_linux_process_fd_summary_t);

    size_t consumed = 0;
    for (int i = 0; i < ITEM_COUNT; i++) {

      const char *file_str = state->top10_file_counts[i];
      if (file_str[0] == '\0') {
        continue;
      }

      size_t remaining = fd_summary_max_size - consumed;
      file_entry_t *file_entry = file_table_get(state->file_table, file_str);

      /* FD summary data is formatted like this:
       *
       *     <count><NULL><file_name><NULL><count><NULL><file_name>...
       *
       * So, snprintf will provide a trailing NULL terminator.
       **/
      int _consumed = snprintf(&fd_summary->most_opened[consumed],
                               remaining,
                               "%d%c%s",
                               file_entry->fd_count,
                               '\0',
                               file_entry->file_str);
      if (_consumed < 0) {
        PK_LOG_ANNO(LOG_ERR, "sprintf failed: %s", strerror(errno));
        break;
      }

      consumed += int_to_sizet(_consumed) + 1 /* NULL terminator */;

      QPF_DEBUG_LOG("summary: %zu: %d %s bytes",
                    consumed,
                    file_entry->fd_count,
                    file_entry->file_str);

      if (consumed > fd_summary_max_size) {
        consumed = fd_summary_max_size;
        fd_summary->most_opened[consumed - 2] = '\0';
        fd_summary->most_opened[consumed - 1] = '\0';
        break;
      }
    }

    /* set two NULL to indicate termination */
    if (consumed < fd_summary_max_size) {
      fd_summary->most_opened[consumed++] = '\0';
    }

    *len = (u8)(sizeof(msg_linux_process_fd_summary_t) + consumed);

    break;
  }

  case SEND_DONE: break;

  case SEND_ERROR:
    PK_LOG_ANNO(LOG_ERR, "file descriptor query failed, error code: %d", state->error_code);
    *len = 0;
    return false;

  default:
    PK_LOG_ANNO(LOG_ERR, "file descriptor query: invalid state value: %d", state->send_state);
    return false;
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
  QPF_DEBUG_LOG("'%s' module reported %d processes on the system", QUERY_SYS_STATE_NAME, pid_count);

  if (pid_count > MAX_PROCESS_COUNT) {
    PK_LOG_ANNO(LOG_ERR,
                "detected more than %d processes on the system: %d",
                MAX_PROCESS_COUNT,
                pid_count);
    goto error;
  }

  for (size_t i = 0; i < ITEM_COUNT; i++) {
    state->top10_pid_counts[i] = "";
    state->top10_file_counts[i] = "";
  }

  state->pid_table = NULL;
  state->file_table = NULL;

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
