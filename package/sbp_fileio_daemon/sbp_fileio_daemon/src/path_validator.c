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

static bool validate_path(const char* basedir_path, const char* path);

typedef struct path_node {
  char path[PATH_MAX];
  LIST_ENTRY(path_node) entries;
} path_node_t;

typedef LIST_HEAD(path_nodes_head, path_node) path_nodes_head_t;

struct path_validator_s
{
  char base_paths[4096];
  path_nodes_head_t path_list;
  size_t allowed_count;
};

path_validator_t *path_validator_create(void)
{
  path_validator_t *ctx = malloc(sizeof(path_validator_t));

  if (ctx == NULL)
    return NULL;

  LIST_INIT(&(ctx->path_list));
  ctx->allowed_count = 0;

  return ctx;
}

void path_validator_destroy(path_validator_t **pctx)
{
  if (*pctx == NULL)
    return;

  path_node_t *node;

  while (!LIST_EMPTY(&((*pctx)->path_list))) {
    node = LIST_FIRST(&((*pctx)->path_list));
    LIST_REMOVE(node, entries);
    free(node);
  }

  free(*pctx);
  *pctx = NULL;
}

bool path_validator_check(path_validator_t *ctx, const char* path)
{
  path_node_t *node;

  LIST_FOREACH(node, &ctx->path_list, entries) {
    if(validate_path(node->path, path))
      return true;
  }

  return false;
}

void path_validator_allow_path(path_validator_t *ctx, const char* path)
{
  assert( path != NULL );

  if (strlen(path) == 0) {

    fprintf(stderr, "ERROR: directories allowed for SBP fileio must not be empty.\n");
    exit(EXIT_FAILURE);
  }

  path_node_t *node = malloc(sizeof(path_node_t));
  node->path[0] = '\0';

  strncpy(node->path, path, sizeof(node->path));

  LIST_INSERT_HEAD(&ctx->path_list, node, entries);
  ctx->allowed_count++;
}

size_t path_validator_allowed_count(path_validator_t *ctx)
{
  return ctx->allowed_count;
}

const char* path_validator_base_paths(path_validator_t *ctx)
{
  path_node_t *node = NULL;
  size_t base_paths_remaining = sizeof(ctx->base_paths);

  char *base_paths = ctx->base_paths;
  memset(base_paths, 0, base_paths_remaining);

  // Reduce by 1 to ensure we have a null terminator
  base_paths_remaining -= 1;

  LIST_FOREACH(node, &ctx->path_list, entries) {

    if (base_paths_remaining == 0)
      break;

    FIO_LOG_DEBUG("node->path: %s", node->path);

    char* end = stpncpy(base_paths, node->path, base_paths_remaining);
    if (end[0] != '\0') break;

    size_t step = (end - base_paths);

    base_paths_remaining -= step;
    base_paths += step;

    char* end2 = stpncpy(base_paths, ",", base_paths_remaining);
    if (end2[0] != '\0') break;

    step = (end2 - base_paths);

    base_paths_remaining -= step;
    base_paths += step;
  }

  if (base_paths_remaining != 0) {
    // Truncate the final ','
    size_t base_paths_size = sizeof(ctx->base_paths) - base_paths_remaining;
    ctx->base_paths[base_paths_size - 2] = '\0';
  }

  return ctx->base_paths;
}

static bool validate_path(const char* basedir_path, const char* path)
{
  FIO_LOG_DEBUG("Checking path: %s against base dir: %s", path, basedir_path);

  char basedir_buf[PATH_MAX] = {0};
  strncpy(basedir_buf, basedir_path, sizeof(basedir_buf));

  if (basedir_buf[strlen(basedir_buf) - 1] == '/')
    basedir_buf[strlen(basedir_buf) - 1] = '\0';

  char _path_buf[PATH_MAX] = {0};
  char* path_buf = _path_buf;

  if (path[0] != '/') {
    path_buf[0] = '/';
    path_buf += 1;
  }

  strncpy(path_buf, path, sizeof(_path_buf) - 1);
  path_buf = _path_buf;

  FIO_LOG_DEBUG("Checking path: %s against base dir: %s", path_buf, basedir_path);

  char realpath_buf[PATH_MAX] = {0};
  struct stat s;

  // Always null terminate so we know if realpath filled in the buffer
  realpath_buf[0] = '\0';

  char* resolved = realpath(path_buf, realpath_buf);
  int error = errno;

  FIO_LOG_DEBUG("Resolved path: %s", resolved);

  if (resolved != NULL) {
    return strstr(resolved, basedir_buf) == resolved;
  }

  if (error == ENOENT && strstr(path_buf, basedir_path) == path_buf) {

    char dirname_buf[PATH_MAX] = {0};
    strncpy(dirname_buf, path_buf, sizeof(dirname_buf));

    // If the errno was "file not found", the prefix matches, and the parent
    //   directory exists, then we allow this path.
    char* parent_dir = dirname(dirname_buf);

    FIO_LOG_DEBUG("Parent dir: %s", dirname_buf);
    return stat(parent_dir, &s) == 0;
  }

  return false;
}
