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

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libsbp/file_io.h>

#include "sbp_fileio.h"
#include "path_validator.h"

#include "fio_debug.h"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

static bool allow_factory_mtd = false;
static bool allow_imageset_bin = false;

#define IMAGESET_BIN_NAME "upgrade.image_set.bin"

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

path_validator_t *g_pv_ctx;

/** Setup file IO
 * Registers relevant SBP callbacks for file IO operations.
 */
void sbp_fileio_setup(path_validator_t *pv_ctx,
                      bool allow_factory_mtd_,
                      bool allow_imageset_bin_,
                      sbp_rx_ctx_t *rx_ctx,
                      sbp_tx_ctx_t *tx_ctx)
{
  g_pv_ctx = pv_ctx;

  allow_factory_mtd = allow_factory_mtd_;
  allow_imageset_bin = allow_imageset_bin_;

  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_REQ, read_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_DIR_REQ, read_dir_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_REMOVE, remove_cb, tx_ctx, NULL);
  sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_WRITE_REQ, write_cb, tx_ctx, NULL);
}

static int allow_mtd_read(const char *path)
{
  // TODO/DAEMON-USERS: Replace this with an SBP message (give firmware a handle
  //    to read here instead of allowing the read).

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
    readlen = 0;
  } else {
    int f = open(msg->filename, O_RDONLY);
    lseek(f, msg->offset, SEEK_SET);
    readlen = read(f, &reply->contents, readlen);
    if (readlen < 0) readlen = 0;
    close(f);
  }

  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_READ_RESP, sizeof(*reply) + readlen, (u8 *)reply);
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
  int write_count = -1;

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

  u8 headerlen = sizeof(*msg) + strlen(msg->filename) + 1;
  int f = open(filename, O_WRONLY | O_CREAT, 0666);
  if (f < 0) {
    piksi_log(LOG_ERR, "Error opening %s for write", filename);
    return;
  }
  if (lseek(f, msg->offset, SEEK_SET) != (off_t)msg->offset) {
    piksi_log(LOG_ERR, "Error seeking to offset %d in %s for write", msg->offset, filename);
    goto cleanup;
  }
  write_count = len - headerlen;
  if (write(f, msg_ + headerlen, write_count) != write_count) {
    piksi_log(LOG_ERR, "Error writing %d bytes to %s", write_count, filename);
    goto cleanup;
  }

  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_WRITE_RESP, sizeof(reply), (u8 *)&reply);

cleanup:
  close(f);
}
