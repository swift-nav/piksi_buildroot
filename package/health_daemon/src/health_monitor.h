/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __HEALTH_THREAD_H
#define __HEALTH_THREAD_H

#include "health_context.h"

typedef struct health_monitor_s health_monitor_t;

typedef int (*health_monitor_init_fn_t)(health_ctx_t* health_ctx);
typedef void (*health_monitor_deinit_fn_t)(void);

typedef struct health_monitor_init_fn_pairs_s {
  health_monitor_init_fn_t init;
  health_monitor_deinit_fn_t deinit;
} health_monitor_init_fn_pair_t;

typedef int (*health_msg_callback_t) (health_monitor_t* monitor, u16 sender_id, u8 len, u8 msg[], void *context);

typedef int (*health_timer_callback_t) (health_monitor_t* monitor, void *context);

/*
 * Set Timer Resolution in ms (Global Value)
 */
void health_monitor_set_timer_resolution(u32 resolution);

/*
 * Reset Timer
 */
void health_monitor_reset_timer(health_monitor_t* monitor);

/*
 * Access shared log function
 */
log_fn_t health_monitor_get_log(health_monitor_t* monitor);

/*
 * Allocate Monitor
 */
health_monitor_t* health_monitor_create(void);

/*
 * Destroy Monitor
 */
void health_monitor_destroy(health_monitor_t** monitor_ptr);

/*
 * Initialize Monitor
 */
int health_monitor_init(health_monitor_t* monitor, health_ctx_t* health_ctx,
                       u16 msg_type, health_msg_callback_t msg_cb,
                       u32 timer_period, health_timer_callback_t timer_cb,
                       void *user_data);

#endif /* __HEALTH_THREAD_H */
