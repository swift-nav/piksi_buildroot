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

/**
 * @file    metrics.h
 * @brief   Piksi Metrics API.
 *
 * @defgroup    metrics Piksi Metrics
 * @addtogroup  metrics
 * @{
 */

#ifndef LIBPIKSI_METRICS_H
#define LIBPIKSI_METRICS_H

#include <libpiksi/common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pk_metrics_s pk_metrics_t;

// clang-format off
typedef enum {
  METRICS_STATUS_ERROR_GENERIC    = -8,
  METRICS_STATUS_TOO_MANY_METRICS = -7,
  METRICS_STATUS_SILENT_FAIL      = -6,
  METRICS_STATUS_INVALID_INDEX    = -5,
  METRICS_STATUS_OPEN_ERROR       = -4,
  METRICS_STATUS_PATH_ERROR       = -3,
  METRICS_STATUS_MEMORY_ERROR     = -2,
  METRICS_STATUS_NO_SLOTS         = -1,
  METRICS_STATUS_SUCCESS          =  0,
} pk_metrics_status_t;
// clang-format on

typedef enum {
  METRICS_TYPE_UNKNOWN = 0,
  METRICS_TYPE_S32,
  METRICS_TYPE_S64,
  METRICS_TYPE_U32,
  METRICS_TYPE_U64,
  METRICS_TYPE_F64,
  METRICS_TYPE_TIME,
} pk_metrics_type_t;

typedef struct {
  u64 ns;
} pk_metrics_time_t;

typedef struct pk_metrics_value_s {
  pk_metrics_type_t type;
  union {
    s32 as_s32;
    s64 as_s64;
    u32 as_u32;
    u64 as_u64;
    double as_f64;
    pk_metrics_time_t as_time;
  } data;
} pk_metrics_value_t;

typedef struct {
  pk_metrics_value_t value;
  pk_metrics_t *metrics;
  void *context;
} pk_metrics_update_t;

typedef pk_metrics_value_t (*pk_metrics_updater_fn_t)(pk_metrics_type_t type,
                                                      pk_metrics_value_t current_value,
                                                      pk_metrics_update_t update);

typedef pk_metrics_value_t (*pk_metrics_reset_fn_t)(pk_metrics_type_t type,
                                                    pk_metrics_value_t initial);

typedef struct {
  const char *folder;
  const char *name;
  pk_metrics_type_t type;
  pk_metrics_updater_fn_t updater;
  pk_metrics_reset_fn_t reseter;
  size_t *idx;
  void *context;
} _pk_metrics_table_entry_t;

typedef struct {
  size_t *index_of_num;
  size_t *index_of_dom;
} pk_metrics_average_t;

pk_metrics_t *_pk_metrics_create(void);

pk_metrics_t *pk_metrics_setup(const char *metrics_base_name,
                               const char *metrics_suffix,
                               _pk_metrics_table_entry_t metrics_table[],
                               size_t entry_count);

void pk_metrics_destroy(pk_metrics_t **metrics_loc);

void pk_metrics_flush(const pk_metrics_t *metrics);

pk_metrics_time_t pk_metrics_gettime(void);

pk_metrics_value_t pk_metrics_reset_default(pk_metrics_type_t type, pk_metrics_value_t initial);

pk_metrics_value_t pk_metrics_reset_time(pk_metrics_type_t type, pk_metrics_value_t initial);

ssize_t pk_metrics_add(pk_metrics_t *metrics,
                       const char *path,
                       const char *name,
                       pk_metrics_type_t type,
                       pk_metrics_value_t initial_value,
                       pk_metrics_updater_fn_t updater_fn,
                       pk_metrics_reset_fn_t reset_fn,
                       void *context);

int _pk_metrics_update(pk_metrics_t *metrics, size_t metric_index, int nargs, ...);

int pk_metrics_reset(pk_metrics_t *metrics, size_t metric_index);

int pk_metrics_read(pk_metrics_t *metrics, size_t metric_index, pk_metrics_value_t *value);

pk_metrics_value_t pk_metrics_updater_sum(pk_metrics_type_t type,
                                          pk_metrics_value_t current_value,
                                          pk_metrics_update_t update);

pk_metrics_value_t pk_metrics_updater_count(pk_metrics_type_t type,
                                            pk_metrics_value_t current_value,
                                            pk_metrics_update_t update);

pk_metrics_value_t pk_metrics_updater_average(pk_metrics_type_t type,
                                              pk_metrics_value_t current_value,
                                              pk_metrics_update_t update);

pk_metrics_value_t pk_metrics_updater_max(pk_metrics_type_t type,
                                          pk_metrics_value_t current_value,
                                          pk_metrics_update_t update);

pk_metrics_value_t pk_metrics_updater_delta(pk_metrics_type_t type,
                                            pk_metrics_value_t current_value,
                                            pk_metrics_update_t update);

pk_metrics_value_t pk_metrics_updater_assign(pk_metrics_type_t type,
                                             pk_metrics_value_t current_value,
                                             pk_metrics_update_t update);

const char *pk_metrics_status_text(pk_metrics_status_t status);

// clang-format off
#define NARGS_SEQ(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N
#define NARGS(...) NARGS_SEQ(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define PK_METRICS_UPDATE(TheMetrics, MetricIndex, ...) \
  _pk_metrics_update(TheMetrics, MetricIndex, NARGS(__VA_ARGS__), ## __VA_ARGS__)

#define _DEF_METRICS_CONV_FUNC(TheType, TheParamType, TheEnumType)      \
  __attribute__((always_inline)) inline pk_metrics_value_t              \
  pk_metrics_ ## TheType (TheParamType v) { return (pk_metrics_value_t) \
    { .type = TheEnumType, .data.as_##TheType = v }; \
  }

_DEF_METRICS_CONV_FUNC(u32,  u32, METRICS_TYPE_U32)
_DEF_METRICS_CONV_FUNC(u64,  u64, METRICS_TYPE_U64)
_DEF_METRICS_CONV_FUNC(s32,  s32, METRICS_TYPE_S32)
_DEF_METRICS_CONV_FUNC(s64,  s64, METRICS_TYPE_S64)
_DEF_METRICS_CONV_FUNC(f64,  double, METRICS_TYPE_F64)
_DEF_METRICS_CONV_FUNC(time, pk_metrics_time_t, METRICS_TYPE_TIME)

#undef _DEF_METRICS_CONV_FUNC

#define PK_METRICS_AS_TIME(TheValue) \
  ((pk_metrics_time_t) { .ns = (s64)(TheValue) })

#define PK_METRICS_VALUE(TheValue)                            \
  _Generic((TheValue),                                        \
           u32:               pk_metrics_u32,                 \
           u64:               pk_metrics_u64,                 \
           s32:               pk_metrics_s32,                 \
           s64:               pk_metrics_s64,                 \
           double:            pk_metrics_f64,                 \
           pk_metrics_time_t: pk_metrics_time                 \
           )(TheValue)
// clang-format on

#ifdef __cplusplus
}
#endif

#include <libpiksi/metrics_table.h>

#endif /* LIBPIKSI_METRICS_H */

/** @} */
