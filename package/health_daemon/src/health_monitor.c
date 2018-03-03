/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <czmq.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/settings.h>

#include "health_monitor.h"

#define SBP_PAYLOAD_SIZE_MAX (255u)

#define DEFAULT_HEALTH_THREAD_TIMER_RESOLUTION (1000u)

struct health_monitor_s {
  health_ctx_t *health_ctx;
  u16 msg_type;
  health_msg_callback_t msg_cb;
  zloop_t *loop;
  health_timer_callback_t timer_cb;
  void *timer_handle;
  bool timer_active;
  u32 timer_period;
  u32 timer_calls;
  void *user_data;
};

static u32 health_timer_resolution = DEFAULT_HEALTH_THREAD_TIMER_RESOLUTION;

void health_monitor_set_timer_resolution(u32 resolution)
{
  health_timer_resolution = resolution;
}

/*
 * Evaluate Timer Threshold
 */
static bool health_monitor_timer_past_threshold(health_monitor_t *monitor)
{
  if (monitor->timer_calls * health_timer_resolution
      > monitor->timer_period) {
    monitor->timer_calls = 0;
    return true;
  }
  return false;
}

/*
 * Reset Timer and Count
 */
void health_monitor_reset_timer(health_monitor_t *monitor)
{
  zloop_ticket_reset(monitor->loop, monitor->timer_handle);
  monitor->timer_calls = 0;
}

/*
 * Register a callback for handling a message type
 */
int health_monitor_register_message_handler(health_monitor_t *monitor,
                                            u16 msg_type,
                                            sbp_msg_callback_t callback)
{
  if (monitor == NULL || callback == NULL) {
    return -1;
  }
  sbp_zmq_pubsub_ctx_t *sbp_ctx =
    health_context_get_sbp_ctx(monitor->health_ctx);
  if (sbp_ctx == NULL) {
    return -1;
  }

  sbp_zmq_rx_ctx_t *rx_ctx = sbp_zmq_pubsub_rx_ctx_get(sbp_ctx);
  if (rx_ctx == NULL) {
    return -1;
  }

  if (sbp_zmq_rx_callback_register(rx_ctx, msg_type, callback, monitor, NULL)
      != 0) {
    return -1;
  }

  return 0;
}

/*
 * Add a watch to handle settings changes
 */
int health_monitor_add_setting_watch(health_monitor_t *monitor,
                                     const char *section,
                                     const char *name,
                                     void *var,
                                     size_t var_len,
                                     settings_type_t type,
                                     settings_notify_fn notify,
                                     void *notify_context)
{
  return settings_add_watch(
    health_context_get_settings_ctx(monitor->health_ctx),
    section,
    name,
    var,
    var_len,
    type,
    notify,
    notify_context);
}

/*
 * Call Monitor Message Callback
 */
static void
health_monitor_message_callback(u16 sender_id, u8 len, u8 msg[], void *ctx)
{
  int result = 0;
  health_monitor_t *monitor = (health_monitor_t *)ctx;
  assert(monitor != NULL);

  if (health_context_get_debug(monitor->health_ctx)) {
    piksi_log(LOG_DEBUG, "Health Monitor Message Callback!");
  }
  // Currently only passthrough
  if (monitor->msg_cb != NULL) {
    result = monitor->msg_cb(monitor, sender_id, len, msg, monitor->user_data);
  }

  if (result == 0 && monitor->timer_active) {
    health_monitor_reset_timer(monitor);
  }
}

/*
 * Call Monitor Timer Callback
 */
static int health_monitor_timer_callback(zloop_t *loop, int timer_id, void *arg)
{
  (void)loop;
  (void)timer_id;
  health_monitor_t *monitor = (health_monitor_t *)arg;
  assert(monitor != NULL);

  if (health_context_get_debug(monitor->health_ctx)) {
    piksi_log(LOG_DEBUG, "Health Monitor Timer Callback!");
  }
  monitor->timer_calls++;
  // Currently only passthrough
  if (health_monitor_timer_past_threshold(monitor)) {
    if (health_context_get_debug(monitor->health_ctx)) {
      piksi_log(LOG_DEBUG, "Timer Past Threshold!");
    }
    if (monitor->timer_cb != NULL) {
      monitor->timer_cb(monitor, monitor->user_data);
    }
    monitor->timer_calls = 0;
  }
  monitor->timer_handle =
    zloop_ticket(monitor->loop, health_monitor_timer_callback, monitor);
  return 0;
}

health_monitor_t *health_monitor_create(void)
{
  health_monitor_t *monitor = malloc(sizeof(health_monitor_t));
  if (monitor == NULL) {
    return NULL;
  }
  memset(monitor, 0, sizeof(health_monitor_t));
  return monitor;
}

void health_monitor_destroy(health_monitor_t **monitor_ptr)
{
  if (monitor_ptr == NULL || *monitor_ptr == NULL) {
    return;
  }
  health_monitor_t *monitor = *monitor_ptr;
  if (monitor->timer_handle != NULL) {
    zloop_ticket_delete(monitor->loop, monitor->timer_handle);
  }
  free(monitor);
  *monitor_ptr = NULL;
}

int health_monitor_init(health_monitor_t *monitor,
                        health_ctx_t *health_ctx,
                        u16 msg_type,
                        health_msg_callback_t msg_cb,
                        u32 timer_period,
                        health_timer_callback_t timer_cb,
                        void *user_data)
{
  if (monitor == NULL || health_ctx == NULL) {
    return -1;
  }
  monitor->health_ctx = health_ctx;
  sbp_zmq_pubsub_ctx_t *sbp_ctx =
    health_context_get_sbp_ctx(monitor->health_ctx);

  monitor->msg_type = msg_type;
  monitor->msg_cb = msg_cb;
  if (monitor->msg_type != 0) {
    if (health_monitor_register_message_handler(
          monitor, monitor->msg_type, health_monitor_message_callback)
        != 0) {
      return -1;
    }
  }

  monitor->timer_period = timer_period;
  monitor->timer_cb = timer_cb;
  if (monitor->timer_period != 0) {
    monitor->loop = sbp_zmq_pubsub_zloop_get(sbp_ctx);
    if (monitor->loop == NULL) {
      return -1;
    }

    // Just set this always for now (overkill but save the need for global
    // context)
    zloop_set_ticket_delay(monitor->loop, health_timer_resolution);
    if (monitor->timer_period < health_timer_resolution) {
      sbp_log(LOG_WARNING,
              "Timer period lower than resolution of %d ms!",
              health_timer_resolution);
    }

    /* Use proxy callback for timer returns */
    monitor->timer_handle =
      zloop_ticket(monitor->loop, health_monitor_timer_callback, monitor);
    if (monitor->timer_handle == NULL) {
      return -1;
    }
    monitor->timer_active = true;
  }
  monitor->user_data = user_data;

  return 0;
}
