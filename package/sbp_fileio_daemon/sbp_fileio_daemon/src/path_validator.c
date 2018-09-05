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

#include <unistd.h>
#include <limits.h>

#include <sys/queue.h>

#include "path_validator.h"

struct path_node {
  const char path[PATH_MAX];
  LIST_ENTRY(path_node) entries;
};

typedef LIST_HEAD(path_nodes_head, path_node) path_nodes_head_t;

struct path_validator_s
{
  path_nodes_head_t path_list;
};

path_validator_t *path_validator_create(void)
{
  return NULL;
}

bool path_validator_check(path_validator_t *ctx, const char* path)
{
  (void) ctx;
  (void) path;

  return false;
}

void path_validator_allow_read(path_validator_t *ctx, const char* path)
{
  (void) ctx;
  (void) path;
}

void path_validator_allow_write(path_validator_t *ctx, const char* path)
{
  (void) ctx;
  (void) path;
}
