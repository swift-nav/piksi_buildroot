/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swift-nav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libpiksi/logging.h>

#include "path_validator.h"
#include "fio_debug.h"

static bool validate_path(const char *basedir_path, const char *path);

typedef struct path_node {
  char path[PATH_MAX];
  LIST_ENTRY(path_node) entries;
} path_node_t;

typedef LIST_HEAD(path_nodes_head, path_node) path_nodes_head_t;

struct path_validator_s {
  char *base_paths;
  size_t base_paths_size;
  path_nodes_head_t path_list;
  size_t allowed_count;
};

path_validator_t *path_validator_create(path_validator_cfg_t *cfg)
{
  path_validator_t *ctx = malloc(sizeof(path_validator_t));

  size_t print_buf_size = cfg == NULL ? 4096 : cfg->print_buf_size;

  ctx->base_paths = malloc(print_buf_size);
  ctx->base_paths_size = print_buf_size;

  if (ctx == NULL) return NULL;

  LIST_INIT(&(ctx->path_list));
  ctx->allowed_count = 0;

  return ctx;
}

void path_validator_destroy(path_validator_t **pctx)
{
  if (pctx == NULL || *pctx == NULL) return;

  path_node_t *node;

  while (!LIST_EMPTY(&((*pctx)->path_list))) {
    node = LIST_FIRST(&((*pctx)->path_list));
    LIST_REMOVE(node, entries);
    free(node);
  }

  free((*pctx)->base_paths);
  free(*pctx);

  *pctx = NULL;
}

bool path_validator_check(path_validator_t *ctx, const char *path)
{
  path_node_t *node;

  LIST_FOREACH(node, &ctx->path_list, entries)
  {
    if (validate_path(node->path, path)) return true;
  }

  return false;
}

bool path_validator_allow_path(path_validator_t *ctx, const char *path)
{
  assert(path != NULL);

  if (strlen(path) == 0) {

    fprintf(stderr, "ERROR: directories allowed for SBP fileio must not be empty.\n");
    exit(EXIT_FAILURE);
  }

  path_node_t *node = malloc(sizeof(path_node_t));
  node->path[0] = '\0';

  int count = snprintf(node->path, sizeof(node->path), "%s", path);
  if (count < 0 || count >= (int)sizeof(node->path)) return false;

  LIST_INSERT_HEAD(&ctx->path_list, node, entries);
  ctx->allowed_count++;

  return true;
}

size_t path_validator_allowed_count(path_validator_t *ctx)
{
  return ctx->allowed_count;
}

const char *path_validator_base_paths(path_validator_t *ctx)
{
  path_node_t *node = NULL;
  size_t base_paths_remaining = ctx->base_paths_size;

  char *base_paths = ctx->base_paths;
  memset(base_paths, 0, base_paths_remaining);

  char *elided_indicator = "...,";
  size_t elided_bytes = strlen(elided_indicator) + 1;

  // Reserve space so that (when needed) we can insert the elided indicator
  base_paths_remaining -= elided_bytes;
  assert(base_paths_remaining > 0);

  LIST_FOREACH(node, &ctx->path_list, entries)
  {

    if (base_paths_remaining == 0) break;

    FIO_LOG_DEBUG("node->path: %s", node->path);

    int count = snprintf(base_paths, base_paths_remaining, "%s,", node->path);
    if (count < 0 || count >= (int)base_paths_remaining) {
      count = snprintf(base_paths, elided_bytes, "%s", elided_indicator);
      assert(count == (int)strlen(elided_indicator));
      break;
    }

    base_paths_remaining -= count;
    base_paths += count;
  }

  char *last_comma = strrchr(ctx->base_paths, ',');
  if (last_comma != NULL) *last_comma = '\0';

  return ctx->base_paths;
}

#define CHECK_PATH_BUFFER(TheCount, TheBuf)                                                  \
  if (TheCount >= (int)sizeof(TheBuf)) {                                                     \
    piksi_log(LOG_ERR, "%s path buffer overflow (%s:%d)", __FUNCTION__, __FILE__, __LINE__); \
    return false;                                                                            \
  }

static bool validate_path(const char *basedir_path, const char *path)
{
  FIO_LOG_DEBUG("Checking path: %s against base dir: %s", path, basedir_path);

  char basedir_buf[PATH_MAX] = {0};
  int count = snprintf(basedir_buf, sizeof(basedir_buf), "%s", basedir_path);

  CHECK_PATH_BUFFER(count, basedir_buf);

  if (basedir_buf[strlen(basedir_buf) - 1] == '/') basedir_buf[strlen(basedir_buf) - 1] = '\0';

  char path_buf[PATH_MAX] = {0};

  if (path[0] != '/') {
    count = snprintf(path_buf, sizeof(path_buf), "/%s", path);
  } else {
    count = snprintf(path_buf, sizeof(path_buf), "%s", path);
  }

  CHECK_PATH_BUFFER(count, path_buf);

  FIO_LOG_DEBUG("Checking path: %s against base dir: %s", path_buf, basedir_path);

  char realpath_buf[PATH_MAX] = {0};
  struct stat s;

  // Always null terminate so we know if realpath filled in the buffer
  realpath_buf[0] = '\0';

  char *resolved = realpath(path_buf, realpath_buf);
  int error = errno;

  FIO_LOG_DEBUG("Resolved path: %s", resolved);

  if (resolved != NULL) {
    return strstr(resolved, basedir_buf) == resolved;
  }

  if (error == ENOENT && strstr(path_buf, basedir_path) == path_buf) {

    char dirname_buf[PATH_MAX] = {0};
    count = snprintf(dirname_buf, sizeof(dirname_buf), "%s", path_buf);

    CHECK_PATH_BUFFER(count, path_buf);

    // If the errno was "file not found", the prefix matches, and the parent
    //   directory exists, then we allow this path.
    char *parent_dir = dirname(dirname_buf);

    FIO_LOG_DEBUG("Parent dir: %s", dirname_buf);
    return stat(parent_dir, &s) == 0;
  }

  return false;
}

#undef CHECK_PATH_BUFFER
