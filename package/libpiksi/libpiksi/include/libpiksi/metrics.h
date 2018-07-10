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
} pk_metrics_type_t;

typedef union pk_metrics_value_s {
  s32 s32;
  s64 s64;
  u32 u32;
  u64 u64;
  double f64;
  char str[128];
} pk_metrics_value_t;

typedef pk_metrics_value_t (* pk_metrics_updater_fn_t)(pk_metrics_type_t type,
                                                       pk_metrics_value_t current_value,
                                                       pk_metrics_value_t new_value,
                                                       void *context);

pk_metrics_t * pk_metrics_create(void);

void pk_metrics_destory(pk_metrics_t **metrics_loc);

void pk_metrics_flush(const pk_metrics_t *metrics);

ssize_t pk_metrics_add(pk_metrics_t *metrics,
                       const char* path,
                       const char* name,
                       pk_metrics_type_t type,
                       pk_metrics_value_t initial_value,
                       pk_metrics_updater_fn_t updater_fn);

int pk_metrics_update(pk_metrics_t *metrics,
                      size_t metric_index,
                      pk_metrics_value_t value,
                      void *context);

int pk_metrics_reset(pk_metrics_t *metrics,
                     size_t metric_index);

int pk_metrics_read(pk_metrics_t *metrics,
                    size_t metric_index,
                    pk_metrics_value_t *value);

pk_metrics_value_t pk_metrics_updater_sum(pk_metrics_type_t type,
                                          pk_metrics_value_t current_value,
                                          pk_metrics_value_t new_value,
                                          void *context);

pk_metrics_value_t pk_metrics_updater_assign(pk_metrics_type_t type,
                                             pk_metrics_value_t current_value,
                                             pk_metrics_value_t new_value,
                                             void *context);

const char* pk_metrics_status_text(pk_metrics_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* LIBPIKSI_METRICS_H */

/** @} */
