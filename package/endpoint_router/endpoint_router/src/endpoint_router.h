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

#ifndef SWIFTNAV_ENDPOINT_ROUTER_H
#define SWIFTNAV_ENDPOINT_ROUTER_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <cmph.h>

#include <libpiksi/endpoint.h>

#ifdef __cplusplus
extern "C" {
#endif

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
  uint16_t pub_ept_index;
  uint16_t sub_ept_index;
  forwarding_rule_t *forwarding_rules_list;
  struct port_s *next;
} port_t;

typedef struct {
  const char *name;
  port_t *ports_list;
} router_cfg_t;

#define MAX_PREFIX_LEN 8

typedef struct {
  size_t count;
  size_t prefix_len;
  u8 (*prefixes)[MAX_PREFIX_LEN];
} rule_prefixes_t;

typedef struct {
  u8 prefix[MAX_PREFIX_LEN];
  size_t count;
  uint16_t* endpoint_indexes;
} cached_port_t;

typedef struct router_s router_t;

typedef struct {
  cmph_t *hash;
  cmph_io_adapter_t *cmph_io_adapter;
  cached_port_t *cached_ports;
  size_t accept_ports_count;
  uint16_t* accept_port_indexes;
  rule_prefixes_t *rule_prefixes;
  size_t rule_count;
  uint16_t sub_ept_index;
  struct router_s *router;
} rule_cache_t;

typedef struct {
  port_t *port;
  pk_endpoint_t *endpoint;
  bool is_pub_addr;
  const char* addr;
} endpoint_slot_t;

struct router_s {
  router_cfg_t *router_cfg;
  rule_cache_t *port_rule_cache;
  size_t port_count;
  uint16_t endpoint_slot_count;
  endpoint_slot_t *endpoint_slots;
};

void debug_printf(const char *msg, ...);

typedef void (* match_fn_t)(forwarding_rule_t *forwarding_rule,
                            filter_t *filter,
                            const u8 *data,
                            size_t length,
                            void *context);

typedef int (* load_endpoints_fn_t)(router_cfg_t* cfg);

router_t* router_create(const char *filename);
void router_teardown(router_t **router_loc);

void process_forwarding_rules(forwarding_rule_t *forwarding_rule,
                              const u8 *data,
                              const size_t length,
                              match_fn_t match_fn,
                              void *context);

rule_prefixes_t* extract_rule_prefixes(port_t *port, rule_cache_t *rule_cache);

inline void rule_prefixes_destroy(rule_prefixes_t **p) {
  if (p != NULL && *p != NULL)
    { free((*p)->prefixes); free(*p); *p = NULL; }
}

int router_reader(const u8 *data, const size_t length, void *context);

void populate_endpoint_slots(router_t *router, port_t *port);

typedef int (*endpoint_send_fn_t)(pk_endpoint_t*, const u8*, const size_t);
extern endpoint_send_fn_t endpoint_send_fn;

typedef void (* endpoint_destroy_fn_t)(pk_endpoint_t** p);
extern endpoint_destroy_fn_t endpoint_destroy_fn;

typedef pk_endpoint_t * (* endpoint_create_fn_t)(const char *endpoint, pk_endpoint_type type);
extern endpoint_create_fn_t endpoint_create_fn;

#ifdef __cplusplus
}
#endif

#endif /* SWIFTNAV_ENDPOINT_ROUTER_H */
