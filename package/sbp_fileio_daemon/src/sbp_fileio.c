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

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libsbp/file_io.h>

#include "sbp_fileio.h"
#include "fio_debug.h"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

#define CLEANUP_FD_CACHE_MS 1e3

static const char* basedir_path = NULL;
static bool allow_factory_mtd = false;
static bool allow_imageset_bin = false;
static void *cleanup_timer_handle = NULL;

#define IMAGESET_BIN_NAME "upgrade.image_set.bin"

#define FD_CACHE_COUNT 32
#define CACHE_CLOSE_AGE 3

typedef struct {
  FILE* fp;
  bool cached;
} fd_cache_result_t;

typedef struct {
  const char* mode;
  int oflag;
  mode_t perm;
} open_params_t;

static const open_params_t open_read = {
  .mode = "r", .oflag = O_RDONLY, .perm = 0
};

static const open_params_t open_write = {
  .mode = "w", .oflag = O_WRONLY|O_CREAT, .perm = 0666
};
 
#define OPEN_READ  (&open_read)
#define OPEN_WRITE (&open_write)

typedef struct {
  FILE* fp;
  char path[PATH_MAX];
  char mode[4];
  time_t opened_at;
} fd_cache_t;

static fd_cache_t fd_cache[FD_CACHE_COUNT] = {
  [0 ... 9] = { NULL, "", "", 0 }
};

enum {
  DENY_MTD_READ = 0,
  NO_MTD_READ,
  ALLOW_MTD_READ,
};

static bool validate_path(const char* path);
static int allow_mtd_read(const char* path);
static const char* filter_imageset_bin(const char* filename);

static void read_cb(u16 sender_id, u8 len, u8 msg[], void* context);
static void read_dir_cb(u16 sender_id, u8 len, u8 msg[], void* context);
static void remove_cb(u16 sender_id, u8 len, u8 msg[], void* context);
static void write_cb(u16 sender_id, u8 len, u8 msg[], void* context);

static void purge_fd_cache(pk_loop_t *loop, void *handle, void *context)
{
  (void) context;
  (void) loop;
  (void) handle;

  time_t now = time(NULL);

  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (fd_cache[idx].fp == NULL) continue;
    if ( (now - fd_cache[idx].opened_at) >= CACHE_CLOSE_AGE) {
      FIO_LOG_DEBUG("Closing cached fp (index %d): %p, filename: %s, mode: %s",
                    idx, fd_cache[idx].fp, fd_cache[idx].path, fd_cache[idx].mode);
      fclose(fd_cache[idx].fp);
      fd_cache[idx] = (fd_cache_t) {
        .fp = NULL, .path = "", .mode = "", .opened_at = -1
      };
    }
  }

  pk_loop_timer_reset(cleanup_timer_handle); 
}

static void return_to_cache(fd_cache_result_t *cache_result)
{
  if (!cache_result->cached && cache_result->fp != NULL) {
    fclose(cache_result->fp);
    return;
  }
}

static void remove_cached_handles(const char* path)
{
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (strcmp(fd_cache[idx].path, path) == 0)
    {
      FIO_LOG_DEBUG("Found file handle, removing (index %d): filename: %s, mode: %s",
                    idx, fd_cache[idx].path, fd_cache[idx].mode);
      fclose(fd_cache[idx].fp);
      fd_cache[idx] = (fd_cache_t) {
        .fp = NULL, .path = "", .mode = "", .opened_at = -1
      };
    }
  }
}

static void flush_cached_write_handles(const char* path, const char* mode)
{
  // If we're returning a read handle, but we have open write handles, flush
  //   those write handles before returning the read handle.

  FIO_LOG_DEBUG("Checking for flush: %s, mode: %s", path, mode);

  if (strcmp(mode, OPEN_READ->mode) != 0) return;

  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (strcmp(fd_cache[idx].path, path) == 0
        && strcmp(fd_cache[idx].mode, OPEN_WRITE->mode) == 0)
    {
      FIO_LOG_DEBUG("Found open write handle, flushing (index %d): filename: %s, mode: %s",
                    idx, fd_cache[idx].path, fd_cache[idx].mode);
      fflush(fd_cache[idx].fp);
    }
  }
}

static fd_cache_result_t open_from_cache(const char* path, const open_params_t *params)
{
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (strcmp(fd_cache[idx].path, path) == 0
        && strcmp(fd_cache[idx].mode, params->mode) == 0)
    {
      FIO_LOG_DEBUG("Found cached file (index %d): filename: %s, mode: %s",
                    idx, fd_cache[idx].path, fd_cache[idx].mode);
      fd_cache[idx].opened_at = time(NULL);
      flush_cached_write_handles(path, params->mode);
      return (fd_cache_result_t) { .fp = fd_cache[idx].fp, .cached = true };
    }
  }
  int fd = open(path, params->oflag, params->perm);
  if (fd < 0) return (fd_cache_result_t) { .fp = NULL, .cached = false };
  FILE* fp = fdopen(fd, params->mode);
  assert( fp != NULL );
  bool found_slot = false;
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (fd_cache[idx].fp == NULL) {
      strncpy(fd_cache[idx].path, path, sizeof(fd_cache[idx].path));
      strncpy(fd_cache[idx].mode, params->mode, sizeof(fd_cache[idx].mode));
      fd_cache[idx].fp = fp;
      fd_cache[idx].opened_at = time(NULL);
      found_slot = true;
      break;
    }
  }
  if (!found_slot) {
    piksi_log(LOG_WARNING, "Could not find a cache slot for file descriptor.");
  }
  flush_cached_write_handles(path, params->mode);
  return (fd_cache_result_t) { .fp = fp, .cached = found_slot };
}

/** Setup file IO
 * Registers relevant SBP callbacks for file IO operations.
 */
void sbp_fileio_setup(pk_loop_t *loop,
                      const char *basedir,
                      bool allow_factory_mtd_,
                      bool allow_imageset_bin_,
                      sbp_rx_ctx_t *rx_ctx,
                      sbp_tx_ctx_t *tx_ctx)
{
  basedir_path = basedir;

  allow_factory_mtd = allow_factory_mtd_;
  allow_imageset_bin = allow_imageset_bin_;

  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_REQ,
                           read_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_DIR_REQ,
                           read_dir_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_REMOVE,
                           remove_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_WRITE_REQ,
                           write_cb, tx_ctx, NULL);

  cleanup_timer_handle = pk_loop_timer_add(loop, CLEANUP_FD_CACHE_MS, purge_fd_cache, NULL);
}

static int allow_mtd_read(const char* path)
{
  // TODO/DAEMON-USERS: Replace this with an SBP message (give firmware a handle
  //    to read here instead of allowing the read).

  if (strcmp(path, "/factory/mtd") != 0)
    return NO_MTD_READ;

  return allow_factory_mtd ? ALLOW_MTD_READ : DENY_MTD_READ;
}

static const char* filter_imageset_bin(const char* filename)
{
  if (!allow_imageset_bin)
    return filename;

  if (strcmp(filename, IMAGESET_BIN_NAME) != 0)
    return filename;

  static char path_buf[PATH_MAX];

  size_t printed = snprintf(path_buf,
                            sizeof(path_buf),
                            "%s/%s",
                            basedir_path,
                            IMAGESET_BIN_NAME);
  assert( printed < sizeof(path_buf) );
  return path_buf;
}

static bool validate_path(const char* path)
{
  static char realpath_buf[PATH_MAX] = {0};
  static struct stat s;

  // Always null terminate so we know if realpath filled in the buffer
  realpath_buf[0] = '\0';

  char* resolved = realpath(path, realpath_buf);
  int error = errno;

  if (resolved != NULL)
    return strstr(resolved, basedir_path) == resolved;

  if (error == ENOENT && strstr(realpath_buf, basedir_path) == realpath_buf) {

    // If the errno was "file not found", the prefix matches, and the parent
    //   directory exists, then we allow this path.
    char* parent_dir = dirname(realpath_buf);
    return stat(parent_dir, &s) == 0;
  }

  return false;
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

  FIO_LOG_DEBUG("read request for '%s', seq=%u, off=%u",
                msg->filename, msg->sequence, msg->offset);

  int st = allow_mtd_read(msg->filename);

  if (st == DENY_MTD_READ || (st == NO_MTD_READ && !validate_path(msg->filename))) {
    piksi_log(LOG_WARNING, "Received FILEIO_READ request for path (%s) outside base directory (%s), ignoring...",
              msg->filename, basedir_path);
    readlen = 0;
  } else {
    fd_cache_result_t r = open_from_cache(msg->filename, OPEN_READ);
    if (r.fp != NULL) {
      lseek(fileno(r.fp), msg->offset, SEEK_SET);
      readlen = fread(&reply->contents, 1, readlen, r.fp);
      if (readlen < 0) readlen = 0;
      return_to_cache(&r);
    } else {
      readlen = 0;
    }
  }

  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_READ_RESP,
              sizeof(*reply) + readlen, (u8*)reply);
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

  struct dirent *dirent;
  u32 offset = msg->offset;
  msg_fileio_read_dir_resp_t *reply = alloca(SBP_FRAMING_MAX_PAYLOAD_SIZE);
  reply->sequence = msg->sequence;

  FIO_LOG_DEBUG("read_dir request for '%s', seq=%u, off=%u",
                msg->dirname, msg->sequence, msg->offset);

  if (!validate_path(msg->dirname)) {
    piksi_log(LOG_WARNING, "Received FILEIO_READ_DIR request for path (%s) outside base directory (%s), ignoring...", msg->dirname, basedir_path);
    len = 0;

  } else {

    DIR *dir = opendir(msg->dirname);
    while (offset && (dirent = readdir(dir)))
      offset--;

    len = 0;
    size_t max_len = SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*reply);
    while ((dirent = readdir(dir))) {
      if (strlen(dirent->d_name) > (max_len - len - 1))
        break;
      strcpy((char*)reply->contents + len, dirent->d_name);
      len += strlen(dirent->d_name) + 1;
    }

    closedir(dir);
  }

  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_READ_DIR_RESP,
              sizeof(*reply) + len, (u8*)reply);
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

  const char* filename = filter_imageset_bin((char*)msg);

  FIO_LOG_DEBUG("remove request for '%s'", filename);

  if (!validate_path(filename)) {
    piksi_log(LOG_WARNING, "Received FILEIO_REMOVE request for path (%s) outside base directory (%s), ignoring...", filename, basedir_path);
    return;
  }

  remove_cached_handles(filename);
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

  msg_fileio_write_req_t *msg = (msg_fileio_write_req_t *)msg_;
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  msg_fileio_write_resp_t reply = {.sequence = msg->sequence};
  size_t write_count = 0;

  FIO_LOG_DEBUG("write request for '%s', seq=%u, off=%u",
                msg->filename, msg->sequence, msg->offset);

  if ((len <= sizeof(*msg) + 2) ||
      (strnlen(msg->filename, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*msg)) ==
                              SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*msg))) {
    piksi_log(LOG_WARNING, "Invalid fileio write message!");
    return;
  }

  const char* filename = filter_imageset_bin(msg->filename);
  if (!validate_path(filename)) {
    piksi_log(LOG_WARNING,
              "Received FILEIO_WRITE request for path (%s)"
              " outside base directory (%s), ignoring...",
              filename, basedir_path);
    return;
  }

  u8 headerlen = sizeof(*msg) + strlen(msg->filename) + 1;
  fd_cache_result_t r = open_from_cache(filename, OPEN_WRITE);
  if (r.fp == NULL) {
    piksi_log(LOG_ERR, "Error opening %s for write", filename);
    return;
  }
  if (lseek(fileno(r.fp), msg->offset, SEEK_SET) != (off_t)msg->offset) {
    piksi_log(LOG_ERR,
              "Error seeking to offset %d in %s for write",
              msg->offset,
              filename);
    goto cleanup;
  }
  write_count = len - headerlen;
  if (fwrite(msg_ + headerlen, 1, write_count, r.fp) != write_count) {
    piksi_log(
      LOG_ERR, "Error writing %d bytes to %s", write_count, filename);
    goto cleanup;
  }

  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_WRITE_RESP,
              sizeof(reply), (u8*)&reply);

cleanup:
  return_to_cache(&r);
}
