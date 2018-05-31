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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <libpiksi/endpoint.h>

typedef enum {
  FILTER_ACTION_ACCEPT,
  FILTER_ACTION_REJECT,
} filter_action_t;

typedef struct filter_s {
  filter_action_t action;
  uint8_t *data;
  size_t len;
  struct filter_s *next;
} filter_t;

typedef struct forwarding_rule_s {
  const char *dst_port_name;
  struct port_s *dst_port;
  filter_t *filters_list;
  struct forwarding_rule_s *next;
} forwarding_rule_t;

typedef struct port_s {
  const char *name;
  const char *pub_addr;
  const char *sub_addr;
  pk_endpoint_t *pub_ept;
  pk_endpoint_t *sub_ept;
  forwarding_rule_t *forwarding_rules_list;
  struct port_s *next;
} port_t;

typedef struct {
  const char *name;
  port_t *ports_list;
} router_t;

void debug_printf(const char *msg, ...);

#endif /* SWIFTNAV_ZMQ_ROUTER_H */
