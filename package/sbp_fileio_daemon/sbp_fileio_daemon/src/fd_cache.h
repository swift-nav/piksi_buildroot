/*
 * Copyright (C) 2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swift-nav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_FD_CACHE_H
#define SWIFTNAV_FD_CACHE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief
 */
typedef struct fd_cache_s fd_cache_t;

typedef struct {
  FILE *fp;
  bool cached;
  size_t *offset;
} open_file_t;


/**
 * @brief
 */
typedef struct {
  size_t cache_timeout;
  size_t cache_size;
} fd_cache_cfg_t;

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
void fd_cache_flush(fd_cache_t *ctx, time_t *ref_time);

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

#ifdef __cplusplus
}
#endif

#endif // SWIFTNAV_FD_CACHE_H
