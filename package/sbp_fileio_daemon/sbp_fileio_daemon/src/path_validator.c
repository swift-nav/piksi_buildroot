/*
 * Copyright (C) 2018-2019 Swift Navigation Inc.
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
#include <libpiksi/metrics.h>
#include <libpiksi/table.h>

#include "path_validator.h"
#include "sbp_fileio.h"

#define MAX_CACHE_ENTRIES 512

static bool validate_path(path_validator_t *ctx, const char *basedir_path, const char *path);
static void cache_lookup(table_t *match_cache, const char *path, bool result);

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
  table_t *match_cache;
  char metrics_ident[PATH_MAX];
  pk_metrics_t *metrics;
};

MAKE_TABLE_WRAPPER(path, bool);

#define MI path_validator_metrics_indexes
#define MT path_validator_metrics_table

/* clang-format off */
PK_METRICS_TABLE(MT, MI,
  PK_METRICS_ENTRY("cache/hits",     "per_second", M_U32, M_UPDATE_COUNT, M_RESET_DEF, cache_hits),
  PK_METRICS_ENTRY("stat/calls",     "per_second", M_U32, M_UPDATE_COUNT, M_RESET_DEF, stat_calls),
  PK_METRICS_ENTRY("realpath/calls", "per_second", M_U32, M_UPDATE_COUNT, M_RESET_DEF, realpath_calls)
 )
/* clang-format on */

path_validator_t *path_validator_create(path_validator_cfg_t *cfg)
{
  path_validator_t *ctx = malloc(sizeof(path_validator_t));

  size_t print_buf_size = cfg == NULL ? 4096 : cfg->print_buf_size;

  ctx->base_paths = malloc(print_buf_size);
  ctx->base_paths_size = print_buf_size;

  if (ctx == NULL) return NULL;

  LIST_INIT(&(ctx->path_list));
  ctx->allowed_count = 0;

  ctx->match_cache = table_create(MAX_CACHE_ENTRIES);

  return ctx;
}

bool path_validator_setup_metrics(path_validator_t *ctx, const char *name)
{
  snprintf_assert(ctx->metrics_ident, sizeof(ctx->metrics_ident), "%s/%s", name, "path_validator");
  ctx->metrics = pk_metrics_setup("sbp_fileio", ctx->metrics_ident, MT, COUNT_OF(MT));

  return ctx->metrics != NULL;
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

  if ((*pctx)->match_cache != NULL) {
    table_destroy(&(*pctx)->match_cache);
  }

  if ((*pctx)->metrics != NULL) {
    pk_metrics_destroy(&(*pctx)->metrics);
  }

  free((*pctx)->base_paths);
  free(*pctx);

  *pctx = NULL;
}

static void cache_lookup(table_t *match_cache, const char *path, bool result)
{
  if (table_count(match_cache) >= MAX_CACHE_ENTRIES) {
    return;
  }

  bool *entry = malloc(sizeof(bool));
  *entry = result;

  if (!path_table_put(match_cache, path, entry)) {
    free(entry);
  }
}

bool path_validator_check(path_validator_t *ctx, const char *path)
{
  path_node_t *node;

  bool *cached_entry = path_table_get(ctx->match_cache, path);

  if (cached_entry != NULL && *cached_entry) {
    if (ctx->metrics != NULL) PK_METRICS_UPDATE(ctx->metrics, MI.cache_hits);
    return true;
  }

  LIST_FOREACH(node, &ctx->path_list, entries)
  {
    if (validate_path(ctx, node->path, path)) {
      cache_lookup(ctx->match_cache, path, true);
      return true;
    }
  }

  /* We don't cache negative results, denying a request doesn't need to be fast */
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

  /* Reserve space so that (when needed) we can insert the 'elided' (...) indicator */
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

void path_validator_flush_metrics(path_validator_t *ctx)
{
  pk_metrics_flush(ctx->metrics);

  pk_metrics_reset(ctx->metrics, MI.cache_hits);
  pk_metrics_reset(ctx->metrics, MI.realpath_calls);
  pk_metrics_reset(ctx->metrics, MI.stat_calls);
}

#define CHECK_PATH_BUFFER(TheCount, TheBuf)                                                  \
  if ((TheCount) >= (int)sizeof((TheBuf))) {                                                 \
    piksi_log(LOG_ERR, "%s path buffer overflow (%s:%d)", __FUNCTION__, __FILE__, __LINE__); \
    return false;                                                                            \
  }

static bool validate_path(path_validator_t *ctx, const char *basedir_path, const char *path)
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

  /* Always null terminate so we know if realpath filled in the buffer */
  realpath_buf[0] = '\0';

  if (ctx->metrics != NULL) PK_METRICS_UPDATE(ctx->metrics, MI.realpath_calls);

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

    /* If the errno was "file not found", the prefix matches, and the parent
     *   directory exists, then we allow this path.
     */
    char *parent_dir = dirname(dirname_buf);

    FIO_LOG_DEBUG("Parent dir: %s", dirname_buf);
    if (ctx->metrics != NULL) PK_METRICS_UPDATE(ctx->metrics, MI.stat_calls);

    return stat(parent_dir, &s) == 0;
  }

  return false;
}

#undef CHECK_PATH_BUFFER
