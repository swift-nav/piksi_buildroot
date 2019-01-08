/*
 * Copyright (C) 2014-2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <libgen.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <libsbp/file_io.h>

#include <libpiksi/logging.h>
#include <libpiksi/metrics.h>
#include <libpiksi/util.h>

#include "sbp_fileio.h"
#include "path_validator.h"

#include "fio_debug.h"

#define WAIT_FLUSH_RETRY_MS 50
#define MS_TO_NS(MS) ((MS)*1e6)

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

#define CLEANUP_FD_CACHE_MS 1e3

static bool allow_factory_mtd = false;
static bool allow_imageset_bin = false;
static void *cleanup_timer_handle = NULL;

#define TEST_CTRL_FILE_TEMPLATE "/var/run/sbp_fileio_%s/test"
static char sbp_fileio_test_control[PATH_MAX] = {0};

#define FLUSH_PID_FILE_TEMPLATE "/var/run/sbp_fileio_%s/pid"
static char sbp_fileio_pid_file[PATH_MAX] = {0};

#define WAIT_FLUSH_FILE_TEMPLATE "/var/run/sbp_fileio_%s/wait.flush"
static char sbp_fileio_wait_flush_file[PATH_MAX] = {0};

#define IMAGESET_BIN_NAME "upgrade.image_set.bin"

#define FD_CACHE_COUNT 32
#define CACHE_CLOSE_AGE 3

#define MI fileio_metrics_indexes
#define MT fileio_metrics_table
#define MR fileio_metrics

extern bool no_cache;

/* clang-format off */
PK_METRICS_TABLE(MT, MI,
  PK_METRICS_ENTRY("data/write/bytes",    "per_second",  M_U32,   M_UPDATE_SUM,   M_RESET_DEF,  write_bytes),
  PK_METRICS_ENTRY("data/read/bytes",    "per_second",  M_U32,   M_UPDATE_SUM,   M_RESET_DEF,  read_bytes)
 )
/* clang-format on */

static pk_metrics_t *fileio_metrics = NULL;

typedef struct {
  FILE *fp;
  bool cached;
  size_t *offset;
} fd_cache_result_t;

typedef struct {
  FILE *fp;
  char path[PATH_MAX];
  char mode[4];
  time_t opened_at;
  size_t offset;
} fd_cache_t;

enum {
  DENY_MTD_READ = 0,
  NO_MTD_READ,
  ALLOW_MTD_READ,
};

static int allow_mtd_read(const char *path);
static const char *filter_imageset_bin(const char *filename);

static void read_cb(u16 sender_id, u8 len, u8 msg[], void *context);
static void read_dir_cb(u16 sender_id, u8 len, u8 msg[], void *context);
static void remove_cb(u16 sender_id, u8 len, u8 msg[], void *context);
static void write_cb(u16 sender_id, u8 len, u8 msg[], void *context);

static off_t read_offset(fd_cache_result_t r);

enum {
  OP_INCREMENT,
  OP_ASSIGN,
};

static void update_offset(fd_cache_result_t r, off_t offset, int op);

static void flush_fd_cache();
static void purge_fd_cache(pk_loop_t *loop, void *handle, int status, void *context);
static void flush_cached_fd(const char *path, const char *mode);
static fd_cache_result_t open_from_cache(const char *path,
                                         const char *mode,
                                         int oflag,
                                         mode_t perm);

static bool test_control_dir(const char *name);

static fd_cache_t fd_cache[FD_CACHE_COUNT] = {[0 ...(FD_CACHE_COUNT - 1)] = {NULL, "", "", 0, 0}};

path_validator_t *g_pv_ctx;

fd_cache_result_t empty_cache_result = (fd_cache_result_t){
  .fp = NULL,
  .cached = false,
  .offset = NULL,
};

fd_cache_t empty_cache_entry = (fd_cache_t){
  .fp = NULL,
  .path = "",
  .mode = "",
  .opened_at = -1,
  .offset = 0,
};

static off_t read_offset(fd_cache_result_t r)
{
  if (r.offset == NULL) return -1;
  return (off_t)*r.offset;
}

static void update_offset(fd_cache_result_t r, off_t offset, int op)
{
  if (r.offset == NULL) return;

  if (op == OP_INCREMENT)
    (*r.offset) += offset;
  else if (op == OP_ASSIGN)
    (*r.offset) = offset;
  else
    PK_LOG_ANNO(LOG_ERR, "invalid operation: %d", op);
}

static void flush_fd_cache()
{
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {

    if (fd_cache[idx].fp == NULL) continue;

    fclose(fd_cache[idx].fp);
    fd_cache[idx] = empty_cache_entry;
  }
}

static void purge_fd_cache(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)status;
  (void)handle;
  (void)context;

  time_t now = time(NULL);

  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (fd_cache[idx].fp == NULL) continue;
    if ((now - fd_cache[idx].opened_at) >= CACHE_CLOSE_AGE) {
      fclose(fd_cache[idx].fp);
      fd_cache[idx] = empty_cache_entry;
    }
  }

  pk_metrics_flush(MR);

  pk_metrics_reset(MR, MI.read_bytes);
  pk_metrics_reset(MR, MI.write_bytes);

  pk_loop_timer_reset(cleanup_timer_handle);
}

static void return_to_cache(fd_cache_result_t *cache_result)
{
  if (!cache_result->cached && cache_result->fp != NULL) {
    fclose(cache_result->fp);
    return;
  }
}

static void flush_cached_fd(const char *path, const char *mode)
{
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (strncmp(fd_cache[idx].path, path, sizeof(fd_cache[idx].path)) == 0
        && strncmp(fd_cache[idx].mode, mode, sizeof(fd_cache[idx].mode)) == 0) {
      FIO_LOG_DEBUG("Flushing cached write fp (index %d): %p, filename: %s, mode: %s, offset: %d",
                    idx,
                    fd_cache[idx].fp,
                    fd_cache[idx].path,
                    fd_cache[idx].mode,
                    fd_cache[idx].offset);
      fclose(fd_cache[idx].fp);
      fd_cache[idx] = empty_cache_entry;
    }
  }
}

static fd_cache_result_t open_from_cache(const char *path, const char *mode, int oflag, mode_t perm)
{
  if (!no_cache) {
    for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
      if (strncmp(fd_cache[idx].path, path, sizeof(fd_cache[idx].path)) == 0
          && strncmp(fd_cache[idx].mode, mode, sizeof(fd_cache[idx].mode)) == 0) {
        FIO_LOG_DEBUG("Found cached fp (index %d): %p, filename: %s, mode: %s, offset: %d",
                      idx,
                      fd_cache[idx].fp,
                      fd_cache[idx].path,
                      fd_cache[idx].mode,
                      fd_cache[idx].offset);
        fd_cache[idx].opened_at = time(NULL);
        return (fd_cache_result_t){.fp = fd_cache[idx].fp,
                                   .cached = true,
                                   .offset = &fd_cache[idx].offset};
      }
    }
  }
  int fd = open(path, oflag, perm);
  if (fd < 0) return (fd_cache_result_t){.fp = NULL, .cached = false, .offset = NULL};
  FILE *fp = fdopen(fd, mode);
  assert(fp != NULL);
  if (no_cache) {
    return (fd_cache_result_t){.fp = fp, .cached = false, .offset = NULL};
  }
  bool found_slot = false;
  size_t *offset_ptr = NULL;
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (fd_cache[idx].fp == NULL) {
      strncpy(fd_cache[idx].path, path, sizeof(fd_cache[idx].path));
      strncpy(fd_cache[idx].mode, mode, sizeof(fd_cache[idx].mode));
      fd_cache[idx].fp = fp;
      fd_cache[idx].opened_at = time(NULL);
      offset_ptr = &fd_cache[idx].offset;
      found_slot = true;
      break;
    }
  }
  if (!found_slot) {
    piksi_log(LOG_WARNING, "Could not find a cache slot for file descriptor.");
  }
  return (fd_cache_result_t){.fp = fp, .cached = found_slot, .offset = offset_ptr};
}

static bool test_control_dir(const char *name)
{
  snprintf_assert(sbp_fileio_test_control, sizeof(sbp_fileio_test_control), TEST_CTRL_FILE_TEMPLATE, name);
  bool success = file_write_string(sbp_fileio_test_control, "");
  if (success) {
    int rc = unlink(sbp_fileio_test_control);
    if (rc < 0) {
      piksi_log(LOG_ERR, "unlink of test file '%s' failed: %s", sbp_fileio_test_control, strerror(errno));
      success = false;
    }
  } else {
      piksi_log(LOG_ERR, "creation/write of test file '%s' failed: %s", sbp_fileio_test_control, strerror(errno));
  }
  return success;
}

/** Setup file IO
 * Registers relevant SBP callbacks for file IO operations.
 */
bool sbp_fileio_setup(const char *name,
                      pk_loop_t *loop,
                      path_validator_t *pv_ctx,
                      bool allow_factory_mtd_,
                      bool allow_imageset_bin_,
                      sbp_rx_ctx_t *rx_ctx,
                      sbp_tx_ctx_t *tx_ctx)
{
  if (!test_control_dir(name)) {
    return false;
  }

  snprintf_assert(sbp_fileio_pid_file, sizeof(sbp_fileio_pid_file), FLUSH_PID_FILE_TEMPLATE, name);

  char pid_buffer[128] = {0};
  snprintf_assert(pid_buffer, sizeof(pid_buffer), "%d", getpid());

  file_write_string(sbp_fileio_pid_file, pid_buffer);

  g_pv_ctx = pv_ctx;

  allow_factory_mtd = allow_factory_mtd_;
  allow_imageset_bin = allow_imageset_bin_;

  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_REQ, read_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_DIR_REQ, read_dir_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_REMOVE, remove_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_WRITE_REQ, write_cb, tx_ctx, NULL);

  cleanup_timer_handle = pk_loop_timer_add(loop, CLEANUP_FD_CACHE_MS, purge_fd_cache, NULL);

  fileio_metrics = pk_metrics_setup("sbp_fileio", name, MT, COUNT_OF(MT));

  return true;
}

void sbp_fileio_teardown(const char *name)
{
  snprintf_assert(sbp_fileio_pid_file, sizeof(sbp_fileio_pid_file), FLUSH_PID_FILE_TEMPLATE, name);

  int rc = unlink(sbp_fileio_pid_file);
  if (rc < 0) {
    piksi_log(LOG_ERR, "unlink of pid file '%s' failed: %s", sbp_fileio_pid_file, strerror(errno));
  }
}

void sbp_fileio_flush(void)
{
  snprintf_assert(sbp_fileio_wait_flush_file,
                  sizeof(sbp_fileio_wait_flush_file),
                  WAIT_FLUSH_FILE_TEMPLATE,
                  sbp_fileio_name);

  flush_fd_cache();

  int rc = unlink(sbp_fileio_wait_flush_file);
  if (rc < 0) {
    piksi_log(LOG_ERR, "unlink of flag file '%s' failed: %s", sbp_fileio_wait_flush_file, strerror(errno));
  }
}

bool sbp_fileio_request_flush(const char *name)
{
  char pid_buffer[128] = {0};

  snprintf_assert(sbp_fileio_pid_file, sizeof(sbp_fileio_pid_file), FLUSH_PID_FILE_TEMPLATE, name);
  int rc = file_read_string(sbp_fileio_pid_file, pid_buffer, sizeof(pid_buffer));

  if (rc != 0) {
    piksi_log(LOG_ERR, "reading pid file for flush failed");
    return false;
  }

  snprintf_assert(sbp_fileio_wait_flush_file, sizeof(sbp_fileio_wait_flush_file), WAIT_FLUSH_FILE_TEMPLATE, name);

  {
    int fd = open(sbp_fileio_wait_flush_file, O_RDWR|O_CREAT, 0666);
    if (fd < 0) {
      piksi_log(LOG_ERR, "creating flag file '%s' failed: %s", sbp_fileio_wait_flush_file, strerror(errno));
      return false;
    }
    close(fd);
  }

  char wait_pid_buffer[128] = {0};

  snprintf_assert(wait_pid_buffer, sizeof(wait_pid_buffer), "%d", getpid());
  file_write_string(sbp_fileio_wait_flush_file, wait_pid_buffer);

  // Ensure that we can stat the "wait" flag file at least once
  struct stat wait_stat;
  if (stat(sbp_fileio_wait_flush_file, &wait_stat) < 0) {
    piksi_log(LOG_ERR, "calling stat on flag file '%s' failed: %s", sbp_fileio_wait_flush_file, strerror(errno));
    return false;
  }

  pid_t pid = (pid_t) atoi(pid_buffer);
  kill(pid, SIGUSR1);

  bool success = true;

  for (;;) {
    if (stat(sbp_fileio_wait_flush_file, &wait_stat) < 0) {
      if (errno == ENOENT) {
        // An expected error, flag file was removed by the fileio daemon so we are done
        break;
      } else {
        piksi_log(LOG_ERR, "unexpected error while stat'ing flag file '%s': %s", sbp_fileio_wait_flush_file, strerror(errno));
        success = false;
        break;
      }
    } else {
      struct timespec ts = {.tv_nsec = MS_TO_NS(WAIT_FLUSH_RETRY_MS)};
      while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
        /* no-op, just need to retry nanosleep */;
      }
    }
  }

  piksi_log(LOG_INFO, "sbp_fileio_daemon flush for named daemon '%s' done", name);
  return success;
}

static int allow_mtd_read(const char *path)
{
  // TODO/DAEMON-USERS: Replace this with an SBP message, e.g. give the firmware a handle
  //    to read here instead of just allowing the read?

  if (strcmp(path, "/factory/mtd") != 0) return NO_MTD_READ;

  return allow_factory_mtd ? ALLOW_MTD_READ : DENY_MTD_READ;
}

static const char *filter_imageset_bin(const char *filename)
{
  if (!allow_imageset_bin) return filename;

  if (strcmp(filename, IMAGESET_BIN_NAME) != 0) return filename;

  static char path_buf[PATH_MAX];

  size_t printed = snprintf(path_buf, sizeof(path_buf), "/data/%s", IMAGESET_BIN_NAME);
  assert(printed < sizeof(path_buf));

  return path_buf;
}

/** File read callback.
 * Responds to a SBP_MSG_FILEIO_READ_REQ message.
 *
 * Reads a certain length (up to 255 bytes) from a given offset. Returns the
 * data in a SBP_MSG_FILEIO_READ_RESP message where the message length field
 * indicates how many bytes were succesfully read.
 */
static void read_cb(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;

  int read_result = 0;
  int seek_result = 0;
  fd_cache_result_t cached_fd = empty_cache_result;

  msg_fileio_read_req_t *msg = (msg_fileio_read_req_t *)msg_;
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  if ((len <= sizeof(*msg)) || (len == SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio read message!");
    return;
  }

  /* Add a null termination to filename */
  msg_[len] = 0;

  msg_fileio_read_resp_t *reply;
  int readlen = SWFT_MIN(msg->chunk_size, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*reply));
  reply = alloca(sizeof(msg_fileio_read_resp_t) + readlen);
  reply->sequence = msg->sequence;

  FIO_LOG_DEBUG("read request for '%s', seq=%u, off=%u", msg->filename, msg->sequence, msg->offset);

  int st = allow_mtd_read(msg->filename);

  if (st == DENY_MTD_READ
      || (st == NO_MTD_READ && !path_validator_check(g_pv_ctx, msg->filename))) {

    piksi_log(LOG_WARNING,
              "Received FILEIO_READ request for path (%s) outside base directory (%s), ignoring...",
              msg->filename,
              path_validator_base_paths(g_pv_ctx));

    read_result = 0;

  } else {

    flush_cached_fd(msg->filename, "w");
    cached_fd = open_from_cache(msg->filename, "r", O_RDONLY, 0);

    if (cached_fd.fp != NULL) {

      if (read_offset(cached_fd) != (off_t)msg->offset) {
        seek_result = fseek(cached_fd.fp, msg->offset, SEEK_SET);
        if (seek_result == 0) {
          update_offset(cached_fd, msg->offset, OP_ASSIGN);
        } else {
          PK_LOG_ANNO(LOG_ERR, "fseek failed: %s (%d)", strerror(errno), errno);
        }
      }

      read_result = fread(&reply->contents, 1, readlen, cached_fd.fp);
      if (read_result != readlen) PK_LOG_ANNO(LOG_ERR, "fread failed: %s", strerror(errno));

      return_to_cache(&cached_fd);
    }
  }

  update_offset(cached_fd, read_result, OP_INCREMENT);
  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_READ_RESP, sizeof(*reply) + read_result, (u8 *)reply);

  PK_METRICS_UPDATE(MR, MI.read_bytes, PK_METRICS_VALUE((u32)read_result));
}

/** Directory listing callback.
 * Responds to a SBP_MSG_FILEIO_READ_DIR_REQ message.
 *
 * The offset parameter can be used to skip the first n elements of the file
 * list.
 *
 * Returns a SBP_MSG_FILEIO_READ_DIR_RESP message containing the directory
 * listings as a NULL delimited list.
 */
static void read_dir_cb(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  msg_fileio_read_dir_req_t *msg = (msg_fileio_read_dir_req_t *)msg_;
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  if ((len <= sizeof(*msg)) || (len == SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio read dir message!");
    return;
  }

  /* Add a null termination to dirname */
  msg_[len] = 0;

  DIR *dir;
  struct dirent *dirent;
  u32 offset = msg->offset;
  msg_fileio_read_dir_resp_t *reply = alloca(SBP_FRAMING_MAX_PAYLOAD_SIZE);
  reply->sequence = msg->sequence;

  FIO_LOG_DEBUG("read_dir request for '%s', seq=%u, off=%u",
                msg->dirname,
                msg->sequence,
                msg->offset);

  if (!path_validator_check(g_pv_ctx, msg->dirname)) {
    piksi_log(LOG_WARNING,
              "Received FILEIO_READ_DIR request for path (%s) "
              "outside base directory (%s), ignoring...",
              msg->dirname,
              path_validator_base_paths(g_pv_ctx));
    len = 0;

    /* Check that directory exists */
  } else if ((dir = opendir(msg->dirname)) == NULL) {
    piksi_log(LOG_WARNING,
              "Received FILEIO_READ_DIR request for path (%s) "
              "which does not exist, ignoring...",
              msg->dirname);
    len = 0;

  } else {
    while (offset && (dirent = readdir(dir)))
      offset--;

    len = 0;
    size_t max_len = SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*reply);
    while ((dirent = readdir(dir))) {
      if (strlen(dirent->d_name) > (max_len - len - 1)) break;
      strcpy((char *)reply->contents + len, dirent->d_name);
      len += strlen(dirent->d_name) + 1;
    }

    closedir(dir);
  }

  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_READ_DIR_RESP, sizeof(*reply) + len, (u8 *)reply);
}

/* Remove file callback.
 * Responds to a SBP_MSG_FILEIO_REMOVE message.
 */
static void remove_cb(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  (void)sender_id;

  if ((len < 1) || (len == SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio remove message!");
    return;
  }

  /* Add a null termination to filename */
  msg[len] = 0;

  const char *filename = filter_imageset_bin((char *)msg);

  FIO_LOG_DEBUG("remove request for '%s'", filename);

  if (!path_validator_check(g_pv_ctx, filename)) {
    piksi_log(
      LOG_WARNING,
      "Received FILEIO_REMOVE request for path (%s) outside base directory (%s), ignoring...",
      filename,
      path_validator_base_paths(g_pv_ctx));
    return;
  }

  flush_cached_fd(filename, "r");
  flush_cached_fd(filename, "w");

  unlink(filename);
}

/* Write to file callback.
 * Responds to a SBP_MSG_FILEIO_WRITE_REQ message.
 *
 * Writes a certain length (up to 255 bytes) at a given offset. Returns a copy
 * of the original SBP_MSG_FILEIO_WRITE_RESP message to check integrity of
 * the write.
 */
static void write_cb(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  u8 headerlen = 0;

  msg_fileio_write_req_t *msg = (msg_fileio_write_req_t *)msg_;
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  msg_fileio_write_resp_t reply = {.sequence = msg->sequence};
  size_t write_count = 0;

  FIO_LOG_DEBUG("write request for '%s', seq=%u, off=%u",
                msg->filename,
                msg->sequence,
                msg->offset);

  if ((len <= sizeof(*msg) + 2)
      || (strnlen(msg->filename, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*msg))
          == SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*msg))) {
    piksi_log(LOG_WARNING, "Invalid fileio write message!");
    return;
  }

  const char *filename = filter_imageset_bin(msg->filename);
  if (!path_validator_check(g_pv_ctx, filename)) {
    piksi_log(
      LOG_WARNING,
      "Received FILEIO_WRITE request for path (%s) outside base directory (%s), ignoring...",
      filename,
      path_validator_base_paths(g_pv_ctx));
    return;
  }

  flush_cached_fd(filename, "r");
  fd_cache_result_t r = open_from_cache(filename, "w", O_WRONLY | O_CREAT, 0666);

  if (r.fp == NULL) {
    piksi_log(LOG_ERR, "Error opening %s for write", filename);
    return;
  }

  if (read_offset(r) != (off_t)msg->offset) {
    if (fseek(r.fp, msg->offset, SEEK_SET) == 0) {
      update_offset(r, msg->offset, OP_ASSIGN);
    } else {
      piksi_log(LOG_ERR,
                "Error seeking to offset %d in %s for write: %s (%d)",
                msg->offset,
                filename,
                strerror(errno),
                errno);
      goto cleanup;
    }
  }

  headerlen = sizeof(*msg) + strlen(msg->filename) + 1;
  write_count = len - headerlen;

  if (fwrite(msg_ + headerlen, 1, write_count, r.fp) != write_count) {
    piksi_log(LOG_ERR, "Error writing %d bytes to %s", write_count, filename);
    goto cleanup;
  }

  update_offset(r, write_count, OP_INCREMENT);

  PK_METRICS_UPDATE(MR, MI.write_bytes, PK_METRICS_VALUE((u32)write_count));
  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_WRITE_RESP, sizeof(reply), (u8 *)&reply);

cleanup:
  return_to_cache(&r);
}
