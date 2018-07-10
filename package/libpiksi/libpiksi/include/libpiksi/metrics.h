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

typedef enum {
  METRICS_STATUS_SILENT_FAIL   = -6,
  METRICS_STATUS_INVALID_INDEX = -5,
  METRICS_STATUS_OPEN_ERROR    = -4,
  METRICS_STATUS_PATH_ERROR    = -3,
  METRICS_STATUS_MEMORY_ERROR  = -2,
  METRICS_STATUS_NO_SLOTS      = -1,
  METRICS_STATUS_SUCCESS       =  0,
} pk_metrics_status_t;

typedef enum {
  METRICS_TYPE_UNKNOWN = 0,
  METRICS_TYPE_S32,
  METRICS_TYPE_S64,
  METRICS_TYPE_U32,
  METRICS_TYPE_U64,
  METRICS_TYPE_F64,
  METRICS_TYPE_STR,
  METRICS_TYPE_TIME,
} pk_metrics_type_t;

typedef struct { u64 ns; } pk_metrics_time_t;

typedef union pk_metrics_value_s {
  s32 s32;
  s64 s64;
  u32 u32;
  u64 u64;
  double f64;
  char str[128];
  pk_metrics_time_t time;
} pk_metrics_value_t;

typedef struct {
  pk_metrics_value_t value;
  void *context;
} pk_metrics_update_t;

typedef pk_metrics_value_t (* pk_metrics_updater_fn_t)(pk_metrics_type_t type,
                                                       pk_metrics_value_t current_value,
                                                       pk_metrics_update_t update);

typedef pk_metrics_value_t (* pk_metrics_reset_fn_t)(pk_metrics_type_t type,
                                                     pk_metrics_value_t initial);


pk_metrics_t * pk_metrics_create(void);

void pk_metrics_destory(pk_metrics_t **metrics_loc);

void pk_metrics_flush(const pk_metrics_t *metrics);

pk_metrics_time_t pk_metrics_gettime();

pk_metrics_value_t pk_metrics_reset_default(pk_metrics_type_t type,
                                            pk_metrics_value_t initial);

pk_metrics_value_t pk_metrics_reset_time(pk_metrics_type_t type,
                                         pk_metrics_value_t initial);

ssize_t pk_metrics_add(pk_metrics_t *metrics,
                       const char* path,
                       const char* name,
                       pk_metrics_type_t type,
                       pk_metrics_value_t initial_value,
                       pk_metrics_updater_fn_t updater_fn,
                       pk_metrics_reset_fn_t reset_fn);

int _pk_metrics_update(pk_metrics_t *metrics,
                       size_t metric_index,
                       int nargs,
                       ...);

int pk_metrics_reset(pk_metrics_t *metrics,
                     size_t metric_index);

int pk_metrics_read(pk_metrics_t *metrics,
                    size_t metric_index,
                    pk_metrics_value_t *value);

pk_metrics_value_t pk_metrics_updater_sum(pk_metrics_type_t type,
                                          pk_metrics_value_t current_value,
                                          pk_metrics_update_t update);

pk_metrics_value_t pk_metrics_updater_count(pk_metrics_type_t type,
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

const char* pk_metrics_status_text(pk_metrics_status_t status);

#define NARGS_SEQ(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N
#define NARGS(...) NARGS_SEQ(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define PK_METRICS_UPDATE(TheMetrics, MetricIndex, ...) \
  _pk_metrics_update(TheMetrics, MetricIndex, NARGS(__VA_ARGS__), ## __VA_ARGS__)

__attribute__((always_inline)) inline pk_metrics_value_t pk_metrics_u32 (u32               v   ) { return (pk_metrics_value_t) { .u32  = v    }; }
__attribute__((always_inline)) inline pk_metrics_value_t pk_metrics_u64 (u64               v   ) { return (pk_metrics_value_t) { .u64  = v    }; }
__attribute__((always_inline)) inline pk_metrics_value_t pk_metrics_s32 (s32               v   ) { return (pk_metrics_value_t) { .s32  = v    }; }
__attribute__((always_inline)) inline pk_metrics_value_t pk_metrics_s64 (s64               v   ) { return (pk_metrics_value_t) { .s64  = v    }; }
__attribute__((always_inline)) inline pk_metrics_value_t pk_metrics_f64 (double            v   ) { return (pk_metrics_value_t) { .f64  = v    }; }
__attribute__((always_inline)) inline pk_metrics_value_t pk_metrics_time(pk_metrics_time_t time) { return (pk_metrics_value_t) { .time = time }; }

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

#ifdef __cplusplus
}
#endif

#endif /* LIBPIKSI_METRICS_H */

/** @} */
