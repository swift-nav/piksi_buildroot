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

typedef struct framer_s framer_t;

typedef enum {
  FILTER_ACTION_ACCEPT, /** A prefix which if matched, will cause data to be forwarded. */
  FILTER_ACTION_REJECT, /** A prefix which if matched, will cause data to be ignored. */
} filter_action_t;

typedef struct filter_s {
  filter_action_t action; /** A filter action, see @c filter_action_t */
  uint8_t *data;          /** The data (prefix) of this filter */
  size_t len;             /** The length of the prefix */
  struct filter_s *next;  /** Pointer to the next filter */
} filter_t;

typedef struct forwarding_rule_s {
  const char *dst_port_name; /** The name of the destination port */
  struct port_s *dst_port;   /** The port that data will be forwarded to */
  filter_t *filters_list;    /** The list of filters that will trigger forwarding rule */
  bool skip_framer;
  struct forwarding_rule_s *next; /** The next fowarding fule */
} forwarding_rule_t;

typedef struct port_s {
  const char *name;       /** The name of the port, used in matching to locate a target port*/
  const char *metric;     /** A metric name to associate with the endpoint */
  const char *pub_addr;   /** The pub socket address that's created for this port, all
                            data forwarded to this port will be emitted from this port */
  const char *sub_addr;   /** The sub socket address for this port, incoming data for this
                            port arrive on this address */
  pk_endpoint_t *pub_ept; /** Endpoint object associated with @c pub_addr */
  pk_endpoint_t *sub_ept; /** Endpoint object associated with @c sub_addr */
  forwarding_rule_t *forwarding_rules_list; /** The list of fowarding rules for this port */
  struct port_s *next;                      /** The next port in the config */
} port_t;

/**
 * Wraps of a named router config and it's list of ports.
 */
typedef struct {
  const char *name;
  port_t *ports_list;
} router_cfg_t;

#define MAX_PREFIX_LEN 8

/** A list of deduped rule prefixes, see @c extract_rule_prefixes */
typedef struct {
  size_t count;
  size_t prefix_len;
  u8 (*prefixes)[MAX_PREFIX_LEN];
} rule_prefixes_t;

typedef struct {
  u8 prefix[MAX_PREFIX_LEN]; /** The prefix of this ports */
  size_t count;              /** The number of endpoints that match this prefix */
  pk_endpoint_t **endpoints; /** The list if endpoints that match this prefix */
} cached_port_t;

typedef struct {
  cmph_t *hash;                       /** Hash that maps from a prefix to a list of ports */
  cmph_io_adapter_t *cmph_io_adapter; /** IO for cmph, we use an in memory vector */
  cached_port_t *cached_ports;        /** An array of prefixes with an array of associated ports */
  size_t accept_ports_count;          /** A count of ports that are "accept everything" ports */
  pk_endpoint_t **accept_ports;       /** The actual ports that default to accepting everything  */
  size_t no_framer_ports_count;       /** Count of the list of ports that skip the framer */
  pk_endpoint_t **no_framer_ports;    /** List of ports that skip the framer */
  rule_prefixes_t *rule_prefixes;     /** A list of all rule prefixes */
  size_t rule_count;                  /** A count of all rules */
  pk_endpoint_t *sub_ept;             /** The SUB enpoint that feeds this rule cache */
  framer_t *framer;                   /** The framer associated with this endpoint */
} rule_cache_t;

typedef struct {
  router_cfg_t *router_cfg;      /** Router config structure */
  rule_cache_t *port_rule_cache; /** A cache structure for each 'SUB' socket in the router config */
  size_t port_count;             /** A count of all SUB ports */
  size_t skip_framer_count; /** How many rule destination ports within the config skip framing */
  size_t accept_last_count; /** How many rule destination ports within the config default to an
                                "accept everything" filter as the last filter. */
} router_t;

void debug_printf(const char *msg, ...);
void router_log(int priority, const char *msg, ...);

/**
 * A function that is invoked with a rule is matched
 */
typedef void (*match_fn_t)(const forwarding_rule_t *forwarding_rule,
                           const filter_t *filter,
                           const u8 *data,
                           size_t length,
                           void *context);

/**
 * A function that creates endpoints in response to a config parse event
 */
typedef int (*load_endpoints_fn_t)(router_cfg_t *cfg, pk_loop_t *loop);

/**
 * Loads a router configuration, builds a rule cache and constructs endpoints.
 */
router_t *router_create(const char *filename, pk_loop_t *loop, load_endpoints_fn_t load_endpoints);

/**
 * Teardown resources allocated by @c router_create
 */
void router_teardown(router_t **router_loc);


/**
 * Process forwarding rules loaded by router_create
 */
void process_forwarding_rules(const forwarding_rule_t *forwarding_rule,
                              const u8 *data,
                              const size_t length,
                              match_fn_t match_fn,
                              void *context);

/**
 * Extract and deduplicated all the prefixes that are specified in a router config.
 */
rule_prefixes_t *extract_rule_prefixes(router_t *router, port_t *port, rule_cache_t *rule_cache);

inline void rule_prefixes_destroy(rule_prefixes_t **p)
{
  if (p != NULL && *p != NULL) {
    free((*p)->prefixes);
    (*p)->prefixes = NULL;
    free(*p);
    *p = NULL;
  }
}

/**
 * A typedef for specifying a send function.
 */
typedef int (*endpoint_send_fn_t)(pk_endpoint_t *, const u8 *, const size_t);

/**
 * Storage for a router 'send' function, used for unit tests.
 */
extern endpoint_send_fn_t endpoint_send_fn;

/**
 * Processes incoming data according to router filter processing rules.
 */
int router_reader(const u8 *data, const size_t length, void *context);

#ifdef __cplusplus
}
#endif

#endif /* SWIFTNAV_ENDPOINT_ROUTER_H */
