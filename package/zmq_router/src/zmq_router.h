/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_ZMQ_ROUTER_H
#define SWIFTNAV_ZMQ_ROUTER_H

#include <stdint.h>
#include <stdbool.h>

#include <czmq.h>

#define FILTER(filter_action, ...)                                            \
  (filter_t) {                                                                \
    .data = (const uint8_t[]){ __VA_ARGS__ },                                 \
    .len = sizeof((const uint8_t[]){ __VA_ARGS__ }),                          \
    .action = filter_action                                                   \
  }

#define FILTER_ACCEPT(...) FILTER(FILTER_ACTION_ACCEPT, __VA_ARGS__ )
#define FILTER_REJECT(...) FILTER(FILTER_ACTION_REJECT, __VA_ARGS__ )

typedef enum {
  FILTER_ACTION_ACCEPT,
  FILTER_ACTION_REJECT,
} filter_action_t;

typedef struct {
  const uint8_t *data;
  int len;
  filter_action_t action;
} filter_t;

typedef struct {
  struct port_t *dst_port;
  const filter_t * const *filters;
} forwarding_rule_t;

typedef struct {
  const char *pub_addr;
  const char *sub_addr;
  const forwarding_rule_t * const *sub_forwarding_rules;
} port_config_t;

typedef struct port_t {
  const port_config_t config;
  zsock_t *pub_socket;
  zsock_t *sub_socket;
} port_t;

typedef struct {
  port_t *ports;
  int ports_count;
} router_t;

#endif /* SWIFTNAV_ZMQ_ROUTER_H */
