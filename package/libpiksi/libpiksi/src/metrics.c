/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <libpiksi/util.h>
#include <libpiksi/logging.h>

#include <libpiksi/metrics.h>

#define MAX_METRICS  32                  /**< Max metrics per pk_metrics_t */
#define METRICS_PATH "/var/log/metrics"  /**< Base metrics path */

typedef struct {
  pk_metrics_type_t type;
  pk_metrics_updater_fn_t updater;
  FILE* stream;
  pk_metrics_value_t initial_value;
  pk_metrics_value_t value;
} metrics_descriptor_t;

struct pk_metrics_s {
  metrics_descriptor_t descriptors[MAX_METRICS];
  size_t count;
};

static int mkpath(char *dir, mode_t mode);
static void write_metric(const metrics_descriptor_t *desc);

pk_metrics_t * pk_metrics_create(void)
{
  pk_metrics_t *metrics = calloc(1, sizeof(pk_metrics_t));
  if (metrics == NULL) return NULL;

  return metrics;
}

void pk_metrics_destory(pk_metrics_t **metrics_loc)
{
  if (metrics_loc == NULL || *metrics_loc == NULL) return;

  pk_metrics_t *metrics = *metrics_loc;

  for (size_t idx = 0; idx < metrics->count; idx++) {
    fclose(metrics->descriptors[idx].stream);
  }

  free(metrics);
  *metrics_loc = NULL;
}

ssize_t pk_metrics_add(pk_metrics_t *metrics,
                       const char* path,
                       const char* name,
                       pk_metrics_type_t type,
                       pk_metrics_value_t initial_value,
                       pk_metrics_updater_fn_t updater_fn)
{
  if (metrics->count >= MAX_METRICS) {
    return METRICS_STATUS_NO_SLOTS;
  }

  metrics_descriptor_t *desc = &metrics->descriptors[metrics->count];

  char metric_dir_path[PATH_MAX];
  size_t count = snprintf(metric_dir_path, sizeof(metric_dir_path), "%s/%s", METRICS_PATH, path);

  if (count >= sizeof(metric_dir_path)) {
    return METRICS_STATUS_MEMORY_ERROR;
  }

  int rc = mkpath(metric_dir_path, 0755);
  if (rc != 0) {
    return METRICS_STATUS_PATH_ERROR;
  }

  char metric_path[PATH_MAX];
  count = snprintf(metric_path, sizeof(metric_path), "%s/%s", metric_dir_path, name);

  if (count >= sizeof(metric_dir_path)) {
    return METRICS_STATUS_MEMORY_ERROR;
  }

  int filedesc = open(metric_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (filedesc < 0) {
    return METRICS_STATUS_OPEN_ERROR;
  }

  desc->stream = fdopen(filedesc, "w+");
  if (desc->stream == NULL) {
    return METRICS_STATUS_OPEN_ERROR;
  }

  desc->type = type;
  desc->updater = updater_fn;
  desc->initial_value = initial_value;
  desc->value = initial_value;

  write_metric(desc);

  return metrics->count++;
}

void pk_metrics_flush(const pk_metrics_t *metrics)
{
  for (size_t idx = 0; idx < metrics->count; idx++) {
    write_metric(&metrics->descriptors[idx]);
    fflush(metrics->descriptors[idx].stream);
  }
}

int pk_metrics_update(pk_metrics_t *metrics,
                      size_t metrics_index,
                      pk_metrics_value_t new_value,
                      void *context)
{
  if (metrics == NULL) {
    return METRICS_STATUS_SILENT_FAIL;
  }

  if (metrics_index >= metrics->count) {
    return METRICS_STATUS_INVALID_INDEX;
  }

  metrics_descriptor_t *desc = &metrics->descriptors[metrics_index];
  desc->value = desc->updater(desc->type, desc->value, new_value, context);

  return METRICS_STATUS_SUCCESS;
}

int pk_metrics_reset(pk_metrics_t *metrics,
                     size_t metrics_index)
{
  if (metrics == NULL) {
    return METRICS_STATUS_SILENT_FAIL;
  }

  if (metrics_index >= metrics->count) {
    return METRICS_STATUS_INVALID_INDEX;
  }

  metrics_descriptor_t *desc = &metrics->descriptors[metrics_index];
  desc->value = desc->initial_value;

  return METRICS_STATUS_SUCCESS;
}

int pk_metrics_read(pk_metrics_t *metrics,
                    size_t metrics_index,
                    pk_metrics_value_t *value)
{
  if (metrics_index >= metrics->count) {
    return METRICS_STATUS_INVALID_INDEX;
  }
  metrics_descriptor_t *desc = &metrics->descriptors[metrics_index];
  *value = desc->value;
}

const char* pk_metrics_status_text(pk_metrics_status_t status)
{
  switch(status) {
  case METRICS_STATUS_SILENT_FAIL:
    return "METRICS_STATUS_SILENT_FAIL";
  case METRICS_STATUS_INVALID_INDEX:
    return "METRICS_STATUS_INVALID_INDEX";
  case METRICS_STATUS_OPEN_ERROR:
    return "METRICS_STATUS_OPEN_ERROR";
  case METRICS_STATUS_PATH_ERROR:
    return "METRICS_STATUS_PATH_ERROR";
  case METRICS_STATUS_MEMORY_ERROR:
    return "METRICS_STATUS_MEMORY_ERROR";
  case METRICS_STATUS_NO_SLOTS:
    return "METRICS_STATUS_NO_SLOTS";
  case METRICS_STATUS_SUCCESS:
    return "METRICS_STATUS_SUCCESS";
  default:
    return "<unknown>";
  }
}

pk_metrics_value_t pk_metrics_updater_sum(pk_metrics_type_t type,
                                          pk_metrics_value_t current_value,
                                          pk_metrics_value_t new_value,
                                          void *context)
{
  (void) context;

  switch(type) {
  case METRICS_TYPE_U32:
    return (pk_metrics_value_t) { .u32 = current_value.u32 + new_value.u32 };
  case METRICS_TYPE_S32:
    return (pk_metrics_value_t) { .s32 = current_value.s32 + new_value.s32 };
  case METRICS_TYPE_U64:
    return (pk_metrics_value_t) { .u64 = current_value.u64 + new_value.u64 };
  case METRICS_TYPE_S64:
    return (pk_metrics_value_t) { .s64 = current_value.s64 + new_value.s64 };
  case METRICS_TYPE_F64:
    return (pk_metrics_value_t) { .f64 = current_value.f64 + new_value.f64 };
  case METRICS_TYPE_STR:
    // TODO: Implement, leave as an error?
  case METRICS_TYPE_UNKNOWN:
  default:
    break;
  }

  assert( false );
}

pk_metrics_value_t pk_metrics_updater_assign(pk_metrics_type_t type,
                                             pk_metrics_value_t current_value,
                                             pk_metrics_value_t new_value,
                                             void *context)
{
  (void) context;
  return new_value;
}

static int mkpath(char *dir, mode_t mode)
{
  struct stat sb;

  if (!dir) {
    errno = EINVAL;
    return 1;
  }

  if (!stat(dir, &sb)) return 0;

  mkpath(dirname(strdupa(dir)), mode);

  return mkdir(dir, mode);
}

static void write_metric(const metrics_descriptor_t *desc)
{
  fseek(desc->stream, 0, SEEK_SET);

  switch(desc->type) {
  case METRICS_TYPE_U32:
    fprintf(desc->stream, "%u\n", desc->value.u32);
    break;
  case METRICS_TYPE_S32:
    fprintf(desc->stream, "%d\n", desc->value.s32);
    break;
  case METRICS_TYPE_U64:
    fprintf(desc->stream, "%llu\n", desc->value.u64);
    break;
  case METRICS_TYPE_S64:
    fprintf(desc->stream, "%lld\n", desc->value.s64);
    break;
  case METRICS_TYPE_F64:
    fprintf(desc->stream, "%f\n", desc->value.s64);
    break;
  case METRICS_TYPE_STR:
    fprintf(desc->stream, "%s\n", desc->value.str);
    break;
  case METRICS_TYPE_UNKNOWN:
  default:
    assert( false );
    break;
  }
}
