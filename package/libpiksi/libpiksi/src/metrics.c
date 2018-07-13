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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <libpiksi/util.h>
#include <libpiksi/logging.h>

#include <libpiksi/metrics.h>

#define MAX_METRICS  32                  /**< Max metrics per pk_metrics_t */
#define METRICS_PATH "/var/log/metrics"  /**< Base metrics path */

typedef struct {
  pk_metrics_type_t type;
  pk_metrics_updater_fn_t update_fn;
  pk_metrics_reset_fn_t reset_fn;
  FILE* stream;
  pk_metrics_value_t initial_value;
  pk_metrics_value_t value;
  void *context;
} metrics_descriptor_t;

struct pk_metrics_s {
  metrics_descriptor_t descriptors[MAX_METRICS];
  size_t count;
};

static int mkpath(char *dir, mode_t mode);
static void write_metric(const metrics_descriptor_t *desc);
static u64 get_denominator_value(pk_metrics_type_t type, const pk_metrics_value_t *value);

pk_metrics_t * _pk_metrics_create(void)
{
  pk_metrics_t *metrics = calloc(1, sizeof(pk_metrics_t));
  if (metrics == NULL) return NULL;

  return metrics;
}

pk_metrics_t* pk_metrics_setup(const char* metrics_base_name,
                               const char* metrics_suffix,
                               _pk_metrics_table_entry_t metrics_table[],
                               size_t entry_count)
{
  size_t count = 0;
  char metrics_folder[PATH_MAX] = {0};
  const char* metrics_name = NULL;
  ssize_t metrics_index = -1;

  pk_metrics_t *metrics = _pk_metrics_create();

  if (metrics == NULL) {

    piksi_log(LOG_ERR, "%s: metrics create failed (%s:%d)", __FUNCTION__, __FILE__, __LINE__);
    return NULL;
  }

  pk_metrics_value_t empty_value = { 0 };

  for (size_t idx = 0; idx < entry_count; idx++) {

    if (strlen(metrics_suffix) > 0) {
      count = snprintf(metrics_folder, sizeof(metrics_folder),
                       "%s_%s/%s", metrics_base_name, metrics_suffix, metrics_table[idx].folder);
    } else {
      count = snprintf(metrics_folder, sizeof(metrics_folder),
                       "%s/%s", metrics_base_name, metrics_table[idx].folder);
    }

    if ( count >= sizeof(metrics_folder) ) {
      piksi_log(LOG_ERR, "%s: metrics folder too large (%s:%d)", __FUNCTION__, __FILE__, __LINE__);
      goto fail2;
    }

    metrics_name = metrics_table[idx].name;

    metrics_index =
      pk_metrics_add(metrics,
                     metrics_folder,
                     metrics_name,
                     metrics_table[idx].type,
                     empty_value,
                     metrics_table[idx].updater,
                     metrics_table[idx].reseter,
                     metrics_table[idx].context);

    if (metrics_index < 0) {
      goto fail;
    }

    *metrics_table[idx].idx = metrics_index;
  }

  return metrics;

fail:
  piksi_log(LOG_ERR, "%s: metrics add for '%s/%s' failed: %s (%s:%d)",
            __FUNCTION__,
            metrics_folder,
            metrics_name,
            pk_metrics_status_text(metrics_index),
            __FILE__,
            __LINE__);
fail2:
  pk_metrics_destroy(&metrics);

  return NULL;
}

void pk_metrics_destroy(pk_metrics_t **metrics_loc)
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
                       pk_metrics_updater_fn_t updater_fn,
                       pk_metrics_reset_fn_t reset_fn,
                       void *context)
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
  desc->update_fn = updater_fn;
  desc->reset_fn = reset_fn;
  desc->initial_value = initial_value;
  desc->value = reset_fn(type, initial_value);
  desc->context = context;

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

int _pk_metrics_update(pk_metrics_t *metrics,
                       size_t metrics_index,
                       int nargs,
                       ...)
{
  if (metrics == NULL) {
    return METRICS_STATUS_SILENT_FAIL;
  }

  if (metrics_index >= metrics->count) {
    return METRICS_STATUS_INVALID_INDEX;
  }

  metrics_descriptor_t *desc = &metrics->descriptors[metrics_index];

  pk_metrics_update_t update;

  update.context = desc->context;
  update.metrics = metrics;

  va_list args;
  va_start(args, nargs);

  if (nargs == 1) update.value = va_arg(args, pk_metrics_value_t);

  va_end(args);

  desc->value = desc->update_fn(desc->type, desc->value, update);

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
  desc->value = desc->reset_fn(desc->type, desc->initial_value);

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

pk_metrics_time_t pk_metrics_gettime()
{
  struct timespec s;

  int rc = clock_gettime(CLOCK_MONOTONIC, &s);

  if ( rc != 0 ) {
    piksi_log(LOG_WARNING, "%s: clock_gettime failed: %s (%s:%d)",
              __FUNCTION__, strerror(errno), __FILE__, __LINE__);

    return (pk_metrics_time_t) { .ns = 0 };
  }

  return (pk_metrics_time_t) { .ns = (s.tv_sec * 1e9) + s.tv_nsec };
}

pk_metrics_value_t pk_metrics_reset_default(pk_metrics_type_t type,
                                            pk_metrics_value_t initial)
{
  return initial;
}

pk_metrics_value_t pk_metrics_reset_time(pk_metrics_type_t type,
                                         pk_metrics_value_t initial)
{
  (void) initial;
  return (pk_metrics_value_t) { .time = pk_metrics_gettime() };
}

pk_metrics_value_t pk_metrics_updater_sum(pk_metrics_type_t type,
                                          pk_metrics_value_t current_value,
                                          pk_metrics_update_t update)
{
  switch(type) {
  case METRICS_TYPE_U32:
    return (pk_metrics_value_t) { .u32 = current_value.u32 + update.value.u32 };
  case METRICS_TYPE_S32:
    return (pk_metrics_value_t) { .s32 = current_value.s32 + update.value.s32 };
  case METRICS_TYPE_U64:
    return (pk_metrics_value_t) { .u64 = current_value.u64 + update.value.u64 };
  case METRICS_TYPE_S64:
    return (pk_metrics_value_t) { .s64 = current_value.s64 + update.value.s64 };
  case METRICS_TYPE_F64:
    return (pk_metrics_value_t) { .f64 = current_value.f64 + update.value.f64 };
  case METRICS_TYPE_TIME:
    return (pk_metrics_value_t) { .time.ns = current_value.time.ns + update.value.time.ns };
  case METRICS_TYPE_UNKNOWN:
  default:
    break;
  }

  piksi_log(LOG_WARNING, "%s: invalid update operation (%s:%d)",
            __FUNCTION__, __FILE__, __LINE__);

  return (pk_metrics_value_t) { 0 };
}

pk_metrics_value_t pk_metrics_updater_delta(pk_metrics_type_t type,
                                            pk_metrics_value_t current_value,
                                            pk_metrics_update_t update)
{
  switch(type) {
  case METRICS_TYPE_U32:
    return (pk_metrics_value_t) { .u32 = update.value.u32 - current_value.u32 };
  case METRICS_TYPE_S32:
    return (pk_metrics_value_t) { .s32 = update.value.s32 - current_value.s32 };
  case METRICS_TYPE_U64:
    return (pk_metrics_value_t) { .u64 = update.value.u64 - current_value.u64 };
  case METRICS_TYPE_S64:
    return (pk_metrics_value_t) { .s64 = update.value.s64 - current_value.s64 };
  case METRICS_TYPE_F64:
    return (pk_metrics_value_t) { .f64 = update.value.f64 - current_value.f64 };
  case METRICS_TYPE_TIME:
    return (pk_metrics_value_t) { .time = update.value.time.ns - current_value.time.ns };
  case METRICS_TYPE_UNKNOWN:
  default:
    break;
  }

  piksi_log(LOG_WARNING, "%s: invalid update operation (%s:%d)",
            __FUNCTION__, __FILE__, __LINE__);

  return (pk_metrics_value_t) { 0 };
}

pk_metrics_value_t pk_metrics_updater_average(pk_metrics_type_t type,
                                              pk_metrics_value_t current_value,
                                              pk_metrics_update_t update)
{
  pk_metrics_average_t *average = update.context;
  pk_metrics_t *metrics = update.metrics;

  if (average->index_of_num == NULL) {
    piksi_log(LOG_WARNING, "%s: invalid update operation: num index NULL (%s:%d)",
              __FUNCTION__, __FILE__, __LINE__);
    return (pk_metrics_value_t) { 0 };
  }

  if (average->index_of_dom == NULL) {
    piksi_log(LOG_WARNING, "%s: invalid update operation: dom index NULL (%s:%d)",
              __FUNCTION__, __FILE__, __LINE__);
    return (pk_metrics_value_t) { 0 };
  }

  if (*average->index_of_num >= metrics->count) {
    piksi_log(LOG_WARNING, "%s: invalid update operation: num index invalid (%s:%d)",
              __FUNCTION__, __FILE__, __LINE__);
    return (pk_metrics_value_t) { 0 };
  }

  if (*average->index_of_dom >= metrics->count) {
    piksi_log(LOG_WARNING, "%s: invalid update operation: dom index invalid (%s:%d)",
              __FUNCTION__, __FILE__, __LINE__);
    return (pk_metrics_value_t) { 0 };
  }

  pk_metrics_value_t num;
  pk_metrics_read(metrics, *average->index_of_num, &num);

  pk_metrics_value_t dom;
  pk_metrics_read(metrics, *average->index_of_dom, &dom);

  pk_metrics_type_t num_type = metrics->descriptors[*average->index_of_num].type;
  pk_metrics_type_t dom_type = metrics->descriptors[*average->index_of_dom].type;

  if (num_type == METRICS_TYPE_UNKNOWN) {
    piksi_log(LOG_WARNING, "%s: invalid update operation: num type invalid (%s:%d)",
              __FUNCTION__, __FILE__, __LINE__);
    return (pk_metrics_value_t) { 0 };
  }

  if (dom_type == METRICS_TYPE_UNKNOWN) {
    piksi_log(LOG_WARNING, "%s: invalid update operation: dom type invalid (%s:%d)",
              __FUNCTION__, __FILE__, __LINE__);
    return (pk_metrics_value_t) { 0 };
  }

  if (num_type != type) {
    piksi_log(LOG_WARNING, "%s: invalid update operation, num type not equal to update type (%s:%d)",
              __FUNCTION__, __FILE__, __LINE__);
    return (pk_metrics_value_t) { 0 };
  }

  // TODO: Find a more intelligent way to mix types

  switch(type) {
  case METRICS_TYPE_U32: {
    u32 dom_value = (u32)get_denominator_value(dom_type, &dom);
    return (pk_metrics_value_t) { .u32 = dom_value == 0 ? 0 : (num.u32 / dom_value) };
  }
  case METRICS_TYPE_S32: {
    s32 dom_value = (s32)get_denominator_value(dom_type, &dom);
    return (pk_metrics_value_t) { .s32 = dom_value == 0 ? 0 : (num.s32 / dom_value) };
  }
  case METRICS_TYPE_U64: {
    u64 dom_value = (u64)get_denominator_value(dom_type, &dom);
    return (pk_metrics_value_t) { .u64 = dom_value == 0 ? 0 : (num.u64 / dom_value) };
  }
  case METRICS_TYPE_S64: {
    s64 dom_value = (s64)get_denominator_value(dom_type, &dom);
    return (pk_metrics_value_t) { .s64 = dom_value == 0 ? 0 : (num.s64 / dom_value) };
  }
  case METRICS_TYPE_F64: {
    double dom_value = (double)get_denominator_value(dom_type, &dom);
    return (pk_metrics_value_t) { .f64 = dom_value == 0 ? 0 : (num.f64 / dom_value) };
  }
  case METRICS_TYPE_TIME: {
    u64 dom_value = (u64)get_denominator_value(dom_type, &dom);
    return (pk_metrics_value_t) { .time.ns = dom_value == 0 ? 0 : (num.time.ns / dom_value) };
  }
  case METRICS_TYPE_UNKNOWN:
  default:
    break;
  }

  piksi_log(LOG_WARNING, "%s: invalid update operation (%s:%d)",
            __FUNCTION__, __FILE__, __LINE__);

  return (pk_metrics_value_t) { 0 };
}

pk_metrics_value_t pk_metrics_updater_max(pk_metrics_type_t type,
                                          pk_metrics_value_t current_value,
                                          pk_metrics_update_t update)
{
  switch(type) {
  case METRICS_TYPE_U32:
    return (pk_metrics_value_t) { .u32 = SWFT_MAX(current_value.u32, update.value.u32) };
  case METRICS_TYPE_S32:
    return (pk_metrics_value_t) { .s32 = SWFT_MAX(current_value.s32, update.value.s32) };
  case METRICS_TYPE_U64:
    return (pk_metrics_value_t) { .u64 = SWFT_MAX(current_value.u64, update.value.u64) };
  case METRICS_TYPE_S64:
    return (pk_metrics_value_t) { .s64 = SWFT_MAX(current_value.s64, update.value.s64) };
  case METRICS_TYPE_F64:
    return (pk_metrics_value_t) { .f64 = SWFT_MAX(current_value.f64, update.value.f64) };
  case METRICS_TYPE_TIME:
    return (pk_metrics_value_t) { .time.ns = SWFT_MAX(current_value.time.ns, update.value.time.ns) };
  case METRICS_TYPE_UNKNOWN:
  default:
    break;
  }

  piksi_log(LOG_WARNING, "%s: invalid update operation (%s:%d)",
            __FUNCTION__, __FILE__, __LINE__);

  return (pk_metrics_value_t) { 0 };
}

pk_metrics_value_t pk_metrics_updater_count(pk_metrics_type_t type,
                                            pk_metrics_value_t current_value,
                                            pk_metrics_update_t update)
{
  switch(type) {
  case METRICS_TYPE_U32:
    return (pk_metrics_value_t) { .u32 = current_value.u32 + 1 };
  case METRICS_TYPE_S32:
    return (pk_metrics_value_t) { .s32 = current_value.s32 + 1 };
  case METRICS_TYPE_U64:
    return (pk_metrics_value_t) { .u64 = current_value.u64 + 1 };
  case METRICS_TYPE_S64:
    return (pk_metrics_value_t) { .s64 = current_value.s64 + 1 };
  case METRICS_TYPE_F64:
    return (pk_metrics_value_t) { .f64 = current_value.f64 + 1 };
  case METRICS_TYPE_TIME:
  case METRICS_TYPE_UNKNOWN:
  default:
    break;
  }

  piksi_log(LOG_WARNING, "%s: invalid update operation (%s:%d)",
            __FUNCTION__, __FILE__, __LINE__);

  return (pk_metrics_value_t) { 0 };
}

pk_metrics_value_t pk_metrics_updater_assign(pk_metrics_type_t type,
                                             pk_metrics_value_t current_value,
                                             pk_metrics_update_t update)
{
  return update.value;
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
  ftruncate(fileno(desc->stream), 0);

  double timeval = 0;

  switch(desc->type) {
  case METRICS_TYPE_U32:
    fprintf(desc->stream, "%u\n", desc->value.u32);
    return;
  case METRICS_TYPE_S32:
    fprintf(desc->stream, "%d\n", desc->value.s32);
    return;
  case METRICS_TYPE_U64:
    fprintf(desc->stream, "%llu\n", desc->value.u64);
    return;
  case METRICS_TYPE_S64:
    fprintf(desc->stream, "%lld\n", desc->value.s64);
    return;
  case METRICS_TYPE_F64:
    fprintf(desc->stream, "%f\n", desc->value.s64);
    return;
  case METRICS_TYPE_TIME:
    timeval = (double)desc->value.time.ns / 1e6;
    fprintf(desc->stream, "%0.03f\n", timeval);
    return;
  case METRICS_TYPE_UNKNOWN:
  default:
    break;
  }

  piksi_log(LOG_WARNING, "%s: invalid metric value operation (%s:%d)",
            __FUNCTION__, __FILE__, __LINE__);
}

static u64 get_denominator_value(pk_metrics_type_t type, const pk_metrics_value_t *value_in)
{
  // TODO: Find some more intelligent way to do this...

  u64 value; 
  switch(type) { 
    case METRICS_TYPE_U32:  value = (u64)value_in->u32;     break;
    case METRICS_TYPE_U64:  value = (u64)value_in->u64;     break;
    case METRICS_TYPE_S32:  value = (u64)value_in->s32;     break;
    case METRICS_TYPE_S64:  value = (u64)value_in->s64;     break;
    case METRICS_TYPE_F64:  value = (u64)value_in->f64;     break;
    case METRICS_TYPE_TIME: value = (u64)value_in->time.ns; break;
    case METRICS_TYPE_UNKNOWN: 
    default: 
       assert( false ); 
  }

  return value;
}
