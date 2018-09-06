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

#include "path_validator.h"

static bool validate_path(const char* basedir_path, const char* path);

typedef struct path_node {
  char path[PATH_MAX];
  LIST_ENTRY(path_node) entries;
} path_node_t;

typedef LIST_HEAD(path_nodes_head, path_node) path_nodes_head_t;

struct path_validator_s
{
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

static bool validate_path(const char* basedir_path, const char* path)
{
  static char realpath_buf[PATH_MAX] = {0};
  static struct stat s;

  // Always null terminate so we know if realpath filled in the buffer
  realpath_buf[0] = '\0';

  char* resolved = realpath(path, realpath_buf);
  int error = errno;

  if (resolved != NULL)
    return strstr(resolved, basedir_path) == resolved;

  if (error == ENOENT && strstr(path, basedir_path) == path) {

    static char dirname_buf[PATH_MAX] = {0};
    strncpy(dirname_buf, path, sizeof(dirname_buf));

    // If the errno was "file not found", the prefix matches, and the parent
    //   directory exists, then we allow this path.
    char* parent_dir = dirname(dirname_buf);
    return stat(parent_dir, &s) == 0;
  }

  return false;
}
