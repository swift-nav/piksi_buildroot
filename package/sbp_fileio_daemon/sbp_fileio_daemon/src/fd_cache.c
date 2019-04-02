/*
 * Copyright (C) 2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "fd_cache.h"

#define FD_CACHE_COUNT 32
#define CACHE_CLOSE_AGE 3

static off_t read_offset(open_file_t r);

enum {
  OP_INCREMENT,
  OP_ASSIGN,
};

static void update_offset(open_file_t r, off_t offset, int op);

static void flush_fd_cache();
static void flush_cached_fd(const char *path, const char *mode);
static open_file_t open_file(const char *path, const char *mode, int oflag, mode_t perm);

open_file_t empty_open_file = (open_file_t){
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

typedef struct {
  FILE *fp;
  char path[PATH_MAX];
  char mode[4];
  time_t opened_at;
  size_t offset;
} cache_entry_t;

struct fd_cache_s {
  cache_entry_t cache[FD_CACHE_COUNT] = {[0 ...(FD_CACHE_COUNT - 1)] = {NULL, "", "", 0, 0}};
};

static off_t read_offset(open_file_t r)
{
  if (r.offset == NULL) return -1;
  return (off_t)*r.offset;
}

static void update_offset(open_file_t r, off_t offset, int op)
{
  if (r.offset == NULL) return;

  if (op == OP_INCREMENT)
    (*r.offset) += offset;
  else if (op == OP_ASSIGN)
    (*r.offset) = offset;
  else
    PK_LOG_ANNO(LOG_ERR, "invalid operation: %d", op);
}

/*
static void flush_fd_cache()
{
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {

    if (fd_cache[idx].fp == NULL) continue;

    fclose(fd_cache[idx].fp);
    fd_cache[idx] = empty_cache_entry;
  }
}
*/

/**
 * @brief
 */
fd_cache_t *fd_cache_create(fd_cache_cfg_t *cfg);

/**
 * @brief
 */
bool fd_cache_setup_metrics(fd_cache_destroy *ctx, const char *name);

/**
 * @brief
 */
void fd_cache_destroy(fd_cache_t **pctx);

/**
 * @brief
 */
void fd_cache_flush(fd_cache_t *ctx, time_t ref_time)
{
  //time_t now = time(NULL);
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (fd_cache[idx].fp == NULL) continue;
    if (ref_time == NULL || (ref_time - fd_cache[idx].opened_at) >= CACHE_CLOSE_AGE) {
      fclose(fd_cache[idx].fp);
      fd_cache[idx] = empty_cache_entry;
    }
  }
}

/**
 * @brief
 */
bool fd_cache_read(fd_cache_t *ctx, const char *path, size_t offset, uint8_t *buf, size_t len);

/**
 * @brief
 */
bool fd_cache_write(fd_cache_t *ctx, const char *path, size_t offset, uint8_t *buf, size_t len);

/**
 * @brief
 */
bool fd_cache_unlink(fd_cache_t *ctx, const char *path);

/**
 * @brief
 */
bool fd_cache_readdir(fd_cache_t *ctx, const char *path, uint8_t *buf, size_t *len);


