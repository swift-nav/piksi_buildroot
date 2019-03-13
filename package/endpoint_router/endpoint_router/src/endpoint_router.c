/*
 * Copyright (C) 2016-2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>

#include <libpiksi/loop.h>
#include <libpiksi/logging.h>
#include <libpiksi/metrics.h>
#include <libpiksi/util.h>
#include <libpiksi/protocols.h>
#include <libpiksi/framer.h>

#include "endpoint_router.h"
#include "endpoint_router_load.h"
#include "endpoint_router_print.h"

#define PROTOCOL_LIBRARY_PATH_ENV_NAME "PROTOCOL_LIBRARY_PATH"
#define PROTOCOL_LIBRARY_PATH_DEFAULT "/usr/lib/endpoint_protocols"

#define PROGRAM_NAME "router"

#define MI metrics_indexes
#define MT message_metrics_table
#define MR router_metrics

static pk_metrics_t *router_metrics = NULL;

/* clang-format off */
PK_METRICS_TABLE(message_metrics_table, MI,

  PK_METRICS_ENTRY("skip_framer/message/count",        "per_second",  M_U32,   M_UPDATE_COUNT,   M_RESET_DEF,  skip_framer_count),
  PK_METRICS_ENTRY("skip_framer/message/total_bypass", "per_second",  M_U32,   M_UPDATE_COUNT,   M_RESET_DEF,  skip_framer_bypass),

  PK_METRICS_ENTRY("endpoint/bytes_dropped",           "per_second",  M_U32,   M_UPDATE_COUNT,   M_RESET_DEF,  bytes_dropped),

  PK_METRICS_ENTRY("message/count",     "per_second",  M_U32,   M_UPDATE_COUNT,   M_RESET_DEF,  count),
  PK_METRICS_ENTRY("message/size",      ".total",      M_U32,   M_UPDATE_SUM,     M_RESET_DEF,  size_total),
  PK_METRICS_ENTRY("message/size",      "per_second",  M_U32,   M_UPDATE_AVERAGE, M_RESET_DEF,  size,
                   M_AVERAGE_OF(MI,     size_total,    count)),
  PK_METRICS_ENTRY("message/wake_ups",  "per_second",  M_U32,   M_UPDATE_COUNT,   M_RESET_DEF,  wakeups),
  PK_METRICS_ENTRY("message/wake_ups",  "max",         M_U32,   M_UPDATE_MAX,     M_RESET_DEF,  wakeups_max),
  PK_METRICS_ENTRY("message/wake_ups",  ".total",      M_U32,   M_UPDATE_COUNT,   M_RESET_DEF,  wakeups_message_count),
  PK_METRICS_ENTRY("message/latency",   "max",         M_TIME,  M_UPDATE_MAX,     M_RESET_DEF,  latency_max),
  PK_METRICS_ENTRY("message/latency",   ".delta",      M_TIME,  M_UPDATE_DELTA,   M_RESET_TIME, latency_delta),
  PK_METRICS_ENTRY("message/latency",   ".total",      M_TIME,  M_UPDATE_SUM,     M_RESET_DEF,  latency_total),
  PK_METRICS_ENTRY("message/latency",   "per_second",  M_TIME,  M_UPDATE_AVERAGE, M_RESET_DEF,  latency,
                   M_AVERAGE_OF(MI,     latency_total, count)),
  PK_METRICS_ENTRY("frame/count",       "per_second",  M_U32,   M_UPDATE_SUM,     M_RESET_DEF,  frame_count),
  PK_METRICS_ENTRY("frame/leftover",    "bytes",       M_U32,   M_UPDATE_SUM,     M_RESET_DEF,  frame_leftovers),

  PK_METRICS_ENTRY("ports/skip_framer", "count",       M_U32,   M_UPDATE_ASSIGN,  M_RESET_DEF,  skip_framer),
  PK_METRICS_ENTRY("ports/accept_last", "count",       M_U32,   M_UPDATE_ASSIGN,  M_RESET_DEF,  accept_last)
 )
/* clang-format on */

static struct {
  const char *filename;
  const char *name;
  bool print;
  bool debug;
  bool process_sbp;
} options = {
  .filename = NULL,
  .name = NULL,
  .print = false,
  .debug = false,
  .process_sbp = false,
};

static void loop_reader_callback(pk_loop_t *loop, void *handle, int status, void *context);
static void process_buffer(rule_cache_t *rule_cache, const u8 *data, const size_t length);
static void process_buffer_via_framer(rule_cache_t *rule_cache,
                                      const u8 *data,
                                      const size_t length);
static void eagain_update_send_metric(pk_endpoint_t *endpoint, size_t bytes_dropped);

endpoint_send_fn_t endpoint_send_fn = NULL;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("-f, --file <config.yml>");
  puts("--name <name>");
  puts("--sbp");
  puts("--print");
  puts("--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_PRINT = 1,
    OPT_ID_NAME,
    OPT_ID_DEBUG,
    OPT_ID_SBP,
  };

  /* clang-format off */
  const struct option long_opts[] = {
    {"file",      required_argument, 0, 'f'},
    {"name",      required_argument, 0, OPT_ID_NAME},
    {"print",     no_argument,       0, OPT_ID_PRINT},
    {"debug",     no_argument,       0, OPT_ID_DEBUG},
    {"sbp",       no_argument,       0, OPT_ID_SBP},
    {0, 0, 0, 0},
  };
  /* clang-format on */

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "f:", long_opts, &opt_index)) != -1) {
    switch (c) {

    case 'f': {
      options.filename = optarg;
    } break;

    case OPT_ID_PRINT: {
      options.print = true;
    } break;

    case OPT_ID_NAME: {
      options.name = optarg;
    } break;

    case OPT_ID_DEBUG: {
      options.debug = true;
    } break;

    case OPT_ID_SBP: {
      options.process_sbp = true;
    } break;

    default: {
      printf("invalid option\n");
      return -1;
    } break;
    }
  }

  if (options.name == NULL) {
    printf("no router instance name given\n");
    return -1;
  }

  if (options.filename == NULL) {
    printf("config file not specified\n");
    return -1;
  }

  return 0;
}

static int router_create_endpoints(router_cfg_t *router, pk_loop_t *loop)
{
  char endpoint_metric[128] = {0};

  port_t *port;

  for (port = router->ports_list; port != NULL; port = port->next) {

    snprintf_assert(endpoint_metric, sizeof(endpoint_metric), "router/%s/pub_server", port->metric);

    port->pub_ept = pk_endpoint_create(pk_endpoint_config()
                                         .endpoint(port->pub_addr)
                                         .identity(endpoint_metric)
                                         .type(PK_ENDPOINT_PUB_SERVER)
                                         .get());
    if (port->pub_ept == NULL) {
      PK_LOG_ANNO(LOG_ERR, "pk_endpoint_create() error\n");
      return -1;
    }

    pk_endpoint_loop_add(port->pub_ept, loop);
    pk_endpoint_eagain_cb_set(port->pub_ept, eagain_update_send_metric);

    snprintf_assert(endpoint_metric, sizeof(endpoint_metric), "router/%s/sub_server", port->metric);

    port->sub_ept = pk_endpoint_create(pk_endpoint_config()
                                         .endpoint(port->sub_addr)
                                         .identity(endpoint_metric)
                                         .type(PK_ENDPOINT_SUB_SERVER)
                                         .get());
    if (port->sub_ept == NULL) {
      PK_LOG_ANNO(LOG_ERR, "pk_endpoint_create() error\n");
      return -1;
    }

    pk_endpoint_loop_add(port->sub_ept, loop);
    pk_endpoint_eagain_cb_set(port->pub_ept, eagain_update_send_metric);
  }

  return 0;
}

static int router_attach(router_t *router, pk_loop_t *loop)
{
  size_t idx = 0;
  port_t *port;

  for (port = router->router_cfg->ports_list; port != NULL; port = port->next, idx++) {

    /* TODO: Add thread/fork here for parallelization [ESD-958] */

    if (pk_loop_endpoint_reader_add(loop,
                                    port->sub_ept,
                                    loop_reader_callback,
                                    &router->port_rule_cache[idx])
        == NULL) {
      PK_LOG_ANNO(LOG_ERR, "pk_loop_endpoint_reader_add error");
      return -1;
    }
  }

  return 0;
}

static void cache_match_process(const forwarding_rule_t *forwarding_rule,
                                const filter_t *filter,
                                const u8 *data,
                                size_t length,
                                void *context)
{
  rule_cache_t *rule_cache = (rule_cache_t *)context;

  assert(length >= rule_cache->rule_prefixes->prefix_len);

  switch (filter->action) {
  case FILTER_ACTION_ACCEPT: {

    if (rule_cache->hash != NULL) {

      uint32_t key =
        cmph_search(rule_cache->hash, (const char *)data, rule_cache->rule_prefixes->prefix_len);
      rule_cache->cached_ports[key].endpoints[rule_cache->cached_ports[key].count++] =
        forwarding_rule->dst_port->pub_ept;
      memcpy(rule_cache->cached_ports[key].prefix, data, rule_cache->rule_prefixes->prefix_len);

    } else {
      /* Port only has one accept rule, no need for a hash, incoming packets should
       *   get picked up by the rule_cache_t->accept_ports list
       */
    }

  } break;

  case FILTER_ACTION_REJECT: {
    /* NOOP */
  } break;

  default: {
    piksi_log(LOG_ERR, "invalid filter action\n");
  } break;
  }
}

static void process_forwarding_rule(const forwarding_rule_t *forwarding_rule,
                                    const u8 *data,
                                    size_t length,
                                    match_fn_t match_fn,
                                    void *context)
{
  /* Iterate over filters for this rule */
  filter_t *filter;
  for (filter = forwarding_rule->filters_list; filter != NULL; filter = filter->next) {

    bool match = false;

    /* Empty filter matches all */
    if (filter->len == 0) {
      match = true;
    } else if (data != NULL) {
      if ((length >= filter->len) && (memcmp(data, filter->data, filter->len) == 0)) {
        match = true;
      }
    }

    if (match) {
      match_fn(forwarding_rule, filter, data, length, context);

      /* Done with this rule after finding a filter match */
      break;
    }
  }
}

static void process_buffer_via_framer(rule_cache_t *rule_cache, const u8 *data, const size_t length)
{
  assert(rule_cache->framer != NULL);

  u32 leftover = 0;

  size_t buffer_index = 0;
  u32 frame_count = 0;

  if (rule_cache->no_framer_ports_count > 0) {
    PK_METRICS_UPDATE(MR, MI.skip_framer_count);
  }

  for (size_t idx = 0; idx < rule_cache->no_framer_ports_count; idx++) {
    endpoint_send_fn(rule_cache->no_framer_ports[idx], data, length);
  }

  if (rule_cache->rule_count == rule_cache->no_framer_ports_count) {
    /* If all we're doing is copying blobs we don't need to de-frame anything */
    PK_METRICS_UPDATE(MR, MI.skip_framer_bypass);
    return;
  }

  while (buffer_index < length) {

    const uint8_t *frame = NULL;
    uint32_t frame_length = 0;

    buffer_index += framer_process(rule_cache->framer,
                                   &data[buffer_index],
                                   length - buffer_index,
                                   &frame,
                                   &frame_length);

    if (frame == NULL) break;

    process_buffer(rule_cache, frame, frame_length);
    frame_count += 1;
  }

  leftover = length - buffer_index;

  PK_METRICS_UPDATE(MR, MI.frame_count, PK_METRICS_VALUE(frame_count));
  PK_METRICS_UPDATE(MR, MI.frame_leftovers, PK_METRICS_VALUE(leftover));
}

static void process_buffer(rule_cache_t *rule_cache, const u8 *data, const size_t length)
{
  size_t prefix_len = rule_cache->rule_prefixes->prefix_len;

  if (length < prefix_len) {
    /* No match, send to all default accept ports */
    for (size_t idx = 0; idx < rule_cache->accept_ports_count; idx++) {
      endpoint_send_fn(rule_cache->accept_ports[idx], data, length);
    }
    return;
  }

  uint32_t key = cmph_search(rule_cache->hash, (const char *)data, prefix_len);
  if (memcmp(rule_cache->cached_ports[key].prefix, data, prefix_len) == 0) {
    /* Match, forward to list of rules */
    for (size_t idx = 0; idx < rule_cache->cached_ports[key].count; idx++) {
      endpoint_send_fn(rule_cache->cached_ports[key].endpoints[idx], data, length);
    }
  } else {
    /* No match, forward to everything that's default accept */
    for (size_t idx = 0; idx < rule_cache->accept_ports_count; idx++) {
      endpoint_send_fn(rule_cache->accept_ports[idx], data, length);
    }
  }
}

int router_reader(const u8 *data, const size_t length, void *context)
{
  PK_METRICS_UPDATE(router_metrics, MI.count);
  PK_METRICS_UPDATE(router_metrics, MI.wakeups_message_count);

  PK_METRICS_UPDATE(router_metrics, MI.size_total, PK_METRICS_VALUE((u32)length));

  rule_cache_t *rule_cache = (rule_cache_t *)context;

  if (options.process_sbp) {
    process_buffer_via_framer(rule_cache, data, length);
  } else {
    process_buffer(rule_cache, data, length);
  }

  return 0;
}

static void eagain_update_send_metric(pk_endpoint_t *endpoint, size_t bytes_dropped)
{
  (void)endpoint;
  PK_METRICS_UPDATE(router_metrics, MI.bytes_dropped, PK_METRICS_VALUE((u32)bytes_dropped));
}

static void pre_receive_metrics()
{
  PK_METRICS_UPDATE(router_metrics, MI.wakeups);

  pk_metrics_reset(router_metrics, MI.latency_delta);
  pk_metrics_reset(router_metrics, MI.wakeups_message_count);
}

static void post_receive_metrics()
{
  pk_metrics_value_t count;
  pk_metrics_read(router_metrics, MI.wakeups_message_count, &count);

  PK_METRICS_UPDATE(router_metrics, MI.wakeups_max, count);
  PK_METRICS_UPDATE(router_metrics, MI.latency_delta, PK_METRICS_VALUE(pk_metrics_gettime()));

  pk_metrics_value_t current_latency;
  pk_metrics_read(router_metrics, MI.latency_delta, &current_latency);

  PK_METRICS_UPDATE(router_metrics, MI.latency_max, current_latency);
  PK_METRICS_UPDATE(router_metrics, MI.latency_total, current_latency);
}

static void loop_reader_callback(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)handle;
  (void)status;

  rule_cache_t *rule_cache = (rule_cache_t *)context;

  pre_receive_metrics();
  pk_endpoint_receive(rule_cache->sub_ept, router_reader, rule_cache);
  post_receive_metrics();
}

void debug_printf(const char *msg, ...)
{
  if (!options.debug) {
    return;
  }

  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);
}

void router_log(int priority, const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  va_list ap2;
  va_copy(ap2, ap);
  vprintf(msg, ap);
  va_end(ap);
  piksi_vlog(priority, msg, ap2);
  va_end(ap2);
}

static int cleanup(int result, pk_loop_t **loop_loc, router_t **router_loc, pk_metrics_t **metrics);

static void loop_1s_metrics(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)status;
  (void)context;

  PK_METRICS_UPDATE(MR, MI.size);
  PK_METRICS_UPDATE(MR, MI.latency);

  pk_metrics_flush(MR);

  pk_metrics_reset(MR, MI.count);
  pk_metrics_reset(MR, MI.size_total);
  pk_metrics_reset(MR, MI.wakeups);
  pk_metrics_reset(MR, MI.wakeups_max);
  pk_metrics_reset(MR, MI.latency);
  pk_metrics_reset(MR, MI.latency_max);
  pk_metrics_reset(MR, MI.latency_total);
  pk_metrics_reset(MR, MI.frame_count);
  pk_metrics_reset(MR, MI.frame_leftovers);
  pk_metrics_reset(MR, MI.skip_framer_count);
  pk_metrics_reset(MR, MI.skip_framer_bypass);
  pk_metrics_reset(MR, MI.bytes_dropped);

  pk_loop_timer_reset(handle);
}

void process_forwarding_rules(const forwarding_rule_t *forwarding_rule,
                              const u8 *data,
                              const size_t length,
                              match_fn_t match_fn,
                              void *context)
{
  for (/* empty */; forwarding_rule != NULL; forwarding_rule = forwarding_rule->next) {
    process_forwarding_rule(forwarding_rule, data, length, match_fn, context);
  }
}

#define MSG_PREFIX_LEN_MISMATCH \
  "ERROR: all forwarding rule prefixes for a port must be the same length (%d vs %zu)\n"

#define MSG_PREFIX_LEN_MAX \
  "ERROR: forwarding rule prefix length (%d) exceeded maximum length (%d)\n"

rule_prefixes_t *extract_rule_prefixes(router_t *router, port_t *port, rule_cache_t *rule_cache)
{
  size_t total_filter_prefixes = 0;
  forwarding_rule_t *rule = NULL;

  int prefix_len = -1;

  for (rule = port->forwarding_rules_list; rule != NULL; rule = rule->next) {

    size_t filter_prefix_count = 0;

    filter_t *filter = NULL;
    filter_t *filter_last = NULL;

    for (filter = rule->filters_list; filter != NULL; filter = filter->next) {

      if (filter->len != 0) {

        filter_prefix_count++;

        if (prefix_len < 0) {
          prefix_len = filter->len;
        } else if (prefix_len != (int)filter->len) {
          fprintf(stderr, MSG_PREFIX_LEN_MISMATCH, prefix_len, filter->len);
          return NULL;
        }

        if (prefix_len > MAX_PREFIX_LEN) {
          fprintf(stderr, MSG_PREFIX_LEN_MAX, prefix_len, MAX_PREFIX_LEN);
          return NULL;
        }
      }

      if (filter->next == NULL) filter_last = filter;
    }

    if (rule->skip_framer) {
      PK_LOG_ANNO(LOG_DEBUG,
                  "adding no framer port: src=%s, dst=%s",
                  port->name,
                  rule->dst_port_name);
      rule_cache->no_framer_ports[rule_cache->no_framer_ports_count++] = rule->dst_port->pub_ept;
      if (router != NULL) router->skip_framer_count++;
    }

    total_filter_prefixes += filter_prefix_count;

    /* Check if the last filter is a "default accept" chain */
    if (filter_last != NULL && filter_last->action == FILTER_ACTION_ACCEPT) {
      rule_cache->accept_ports[rule_cache->accept_ports_count++] = rule->dst_port->pub_ept;
      if (router != NULL) router->accept_last_count++;
    }
  }

  size_t filter_prefix_index = 0;

  u8(*all_filter_prefixes)[MAX_PREFIX_LEN] =
    calloc(total_filter_prefixes, sizeof(u8) * MAX_PREFIX_LEN);
  assert(all_filter_prefixes != NULL);

  STAGE_CLEANUP(all_filter_prefixes, ({ free(all_filter_prefixes); }));

  for (rule = port->forwarding_rules_list; rule != NULL; rule = rule->next) {
    for (filter_t *filter = rule->filters_list; filter != NULL; filter = filter->next) {
      if (filter->data != NULL && filter->len > 0) {
        u8 *filter_prefix = all_filter_prefixes[filter_prefix_index++];
        memcpy(filter_prefix, filter->data, prefix_len);
      }
    }
  }

  assert(total_filter_prefixes == filter_prefix_index);

  int compare_prefixes(const void *a, const void *b)
  {
    return memcmp(a, b, (size_t)prefix_len);
  };

  qsort(all_filter_prefixes, total_filter_prefixes, MAX_PREFIX_LEN, compare_prefixes);

  u8(*deduped_filter_prefixes)[MAX_PREFIX_LEN] =
    calloc(total_filter_prefixes, sizeof(u8) * MAX_PREFIX_LEN);
  assert(deduped_filter_prefixes != NULL);

  STAGE_CLEANUP(deduped_filter_prefixes, ({
                  if (deduped_filter_prefixes != NULL) free(deduped_filter_prefixes);
                }));

  size_t deduped_filter_index = 0;

  u8 *prev_entry = NULL;
  for (size_t filter_idx = 0; filter_idx < total_filter_prefixes; filter_idx++) {
    if (prev_entry != NULL
        && memcmp(prev_entry, all_filter_prefixes[filter_idx], prefix_len) == 0) {
      continue;
    }
    prev_entry = all_filter_prefixes[filter_idx];
    memcpy(deduped_filter_prefixes[deduped_filter_index++],
           all_filter_prefixes[filter_idx],
           prefix_len);
  }

  rule_prefixes_t *rule_prefixes = malloc(sizeof(rule_prefixes_t));
  assert(rule_prefixes != NULL);
  STAGE_CLEANUP(rule_prefixes, ({
                  if (rule_prefixes != NULL) free(rule_prefixes);
                }));

  rule_prefixes->prefixes =
    UNSTAGE_CLEANUP_X(deduped_filter_prefixes, ARRAY_OF_FIXED_BLOCKS(u8, MAX_PREFIX_LEN));
  rule_prefixes->count = deduped_filter_index;
  rule_prefixes->prefix_len = prefix_len;

  return UNSTAGE_CLEANUP(rule_prefixes, rule_prefixes_t *);
}

router_t *router_create(const char *filename, pk_loop_t *loop, load_endpoints_fn_t load_endpoints)
{
  router_t *router = malloc(sizeof(router_t));
  assert(router != NULL);
  STAGE_CLEANUP(router, ({
                  if (router != NULL) free(router);
                }));

  router_cfg_t *router_cfg = router_cfg_load(filename);

  if (router_cfg == NULL) {
    return NULL;
  }

  STAGE_CLEANUP(router_cfg, ({
                  if (router_cfg != NULL) router_cfg_teardown(&router_cfg);
                }));

  if (load_endpoints(router_cfg, loop) != 0) {
    return NULL;
  }

  router->router_cfg = UNSTAGE_CLEANUP(router_cfg, router_cfg_t *);

  router->port_count = 0;
  for (port_t *port = router->router_cfg->ports_list; port != NULL; port = port->next) {
    router->port_count++;
  }

  router->skip_framer_count = 0;
  router->accept_last_count = 0;

  router->port_rule_cache = calloc(router->port_count, sizeof(rule_cache_t));
  assert(router->port_rule_cache != NULL);

  size_t port_index = 0;

  for (port_t *port = router->router_cfg->ports_list; port != NULL; port = port->next) {

    rule_cache_t *rule_cache = &router->port_rule_cache[port_index];

    rule_cache->sub_ept = port->sub_ept;
    rule_cache->rule_count = 0;

    if (options.process_sbp) {
      rule_cache->framer = framer_create("sbp");
      assert(rule_cache->framer != NULL);
    }

    forwarding_rule_t *rules = port->forwarding_rules_list;

    for (/* empty */; rules != NULL; rules = rules->next) {
      rule_cache->rule_count++;
    }

    rule_cache->accept_ports_count = 0;
    rule_cache->no_framer_ports_count = 0;

    rule_cache->accept_ports = calloc(rule_cache->rule_count, sizeof(pk_endpoint_t *));
    assert(rule_cache->accept_ports != NULL);

    rule_cache->no_framer_ports = calloc(rule_cache->rule_count, sizeof(pk_endpoint_t *));
    assert(rule_cache->no_framer_ports != NULL);

    rule_prefixes_t *rule_prefixes = extract_rule_prefixes(router, port, rule_cache);

    if (rule_prefixes == NULL) {
      fprintf(stderr, "ERROR: extract_rule_prefixes failed\n");
      return NULL;
    }

    rule_cache->rule_prefixes = rule_prefixes;

    if (rule_cache->rule_prefixes->count > 0) {
#ifdef DEBUG_ENDPOINT_ROUTER
      fprintf(stderr, "count: %zu\n", rule_cache->rule_prefixes->count);
      fprintf(stderr, "size: %zu\n", sizeof(*rule_prefixes->prefixes));
      fprintf(stderr, "len: %zu\n", rule_prefixes->prefix_len);
#endif
      rule_cache->cmph_io_adapter =
        cmph_io_struct_vector_adapter(/* vector       = */ rule_prefixes->prefixes,
                                      /* struct_size  = */ sizeof(*rule_prefixes->prefixes),
                                      /* key_offset   = */ 0,
                                      /* key_len      = */ rule_prefixes->prefix_len,
                                      /* nkeys        = */ rule_prefixes->count);

      cmph_config_t *config = cmph_config_new(rule_cache->cmph_io_adapter);
      assert(config != NULL);

      cmph_config_set_algo(config, CMPH_BDZ);

      rule_cache->hash = cmph_new(config);
      assert(rule_cache->hash != NULL);

      cmph_config_destroy(config);
#ifdef DEBUG_ENDPOINT_ROUTER
      uint32_t size = cmph_size(rule_cache->hash);
      fprintf(stderr, "cmph_size: %d\n", size);
#endif
      rule_cache->cached_ports = calloc(rule_prefixes->count, sizeof(cached_port_t));
      assert(rule_cache->cached_ports != NULL);

      for (size_t idx = 0; idx < rule_prefixes->count; idx++) {

        rule_cache->cached_ports[idx].endpoints =
          calloc(router->port_count, sizeof(pk_endpoint_t *));
        assert(rule_cache->cached_ports[idx].endpoints != NULL);

        rule_cache->cached_ports[idx].count = 0;
      }

      for (size_t idx = 0; idx < rule_prefixes->count; idx++) {
#ifdef DEBUG_ENDPOINT_ROUTER
        u8 test[] = {0x55, 0xaf, 0x00};
        uint32_t key = cmph_search(rule_cache->hash, (char *)test, 3);
        fprintf(stderr,
                "[%zu]: %02x %02x %02x, len=%zu, key=%d\n",
                idx,
                rule_prefixes->prefixes[idx][0],
                rule_prefixes->prefixes[idx][1],
                rule_prefixes->prefixes[idx][2],
                rule_prefixes->prefix_len,
                key);
#endif
        process_forwarding_rules(port->forwarding_rules_list,
                                 rule_prefixes->prefixes[idx],
                                 rule_prefixes->prefix_len,
                                 cache_match_process,
                                 rule_cache);
      }
    } else {

      rule_cache->cmph_io_adapter = NULL;
      rule_cache->hash = NULL;
      rule_cache->cached_ports = NULL;
    }

    port_index++;
  }

  return UNSTAGE_CLEANUP(router, router_t *);
}

void router_teardown(router_t **router_loc)
{
  if (*router_loc == NULL) return;

  router_t *router = *router_loc;
  router_cfg_teardown(&router->router_cfg);

  for (size_t rule_idx = 0; rule_idx < router->port_count; rule_idx++) {

    rule_cache_t *rule_cache = &router->port_rule_cache[rule_idx];

    if (rule_cache->cached_ports != NULL) {
      for (size_t idx = 0; idx < rule_cache->rule_prefixes->count; idx++) {
        if (rule_cache->cached_ports[idx].endpoints != NULL) {
          free(rule_cache->cached_ports[idx].endpoints);
          rule_cache->cached_ports[idx].endpoints = NULL;
        }
      }
      free(rule_cache->cached_ports);
      rule_cache->cached_ports = NULL;
    }

    if (rule_cache->accept_ports != NULL) {
      free(rule_cache->accept_ports);
      rule_cache->accept_ports = NULL;
    }

    if (rule_cache->no_framer_ports != NULL) {
      free(rule_cache->no_framer_ports);
      rule_cache->no_framer_ports = NULL;
      rule_cache->no_framer_ports_count = 0;
    }

    rule_prefixes_destroy(&rule_cache->rule_prefixes);

    if (rule_cache->hash != NULL) {
      cmph_destroy(rule_cache->hash);
      rule_cache->hash = NULL;
    }

    if (rule_cache->cmph_io_adapter != NULL) {
      cmph_io_struct_vector_adapter_destroy(rule_cache->cmph_io_adapter);
      rule_cache->cmph_io_adapter = NULL;
    }

    if (rule_cache->framer != NULL) {
      framer_destroy(&rule_cache->framer);
    }
  }

  free(router->port_rule_cache);
  free(router);

  *router_loc = NULL;
}

int main(int argc, char *argv[])
{
  pk_loop_t *loop = NULL;
  router_t *router = NULL;

  endpoint_destroy_fn = pk_endpoint_destroy;
  endpoint_send_fn = pk_endpoint_send;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  const char *protocol_library_path = getenv(PROTOCOL_LIBRARY_PATH_ENV_NAME);
  if (protocol_library_path == NULL) {
    protocol_library_path = PROTOCOL_LIBRARY_PATH_DEFAULT;
  }

  if (protocols_import(protocol_library_path) != 0) {
    syslog(LOG_ERR, "error importing protocols");
    fprintf(stderr, "error importing protocols\n");
    exit(EXIT_FAILURE);
  }

  router_metrics = pk_metrics_setup("endpoint_router", options.name, MT, COUNT_OF(MT));
  if (router_metrics == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  /* Create loop */
  loop = pk_loop_create();
  if (loop == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  /* Load router from config file */
  router = router_create(options.filename, loop, router_create_endpoints);
  if (router == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  PK_METRICS_UPDATE(MR, MI.skip_framer, PK_METRICS_VALUE(router->skip_framer_count));
  PK_METRICS_UPDATE(MR, MI.accept_last, PK_METRICS_VALUE(router->accept_last_count));

  /* Print router config and exit if requested */
  if (options.print) {
    if (router_print(stdout, router->router_cfg) != 0) {
      exit(EXIT_FAILURE);
    }
    exit(cleanup(EXIT_SUCCESS, &loop, &router, &router_metrics));
  }

  void *handle = pk_loop_timer_add(loop, 1000, loop_1s_metrics, NULL);
  if (handle == NULL) {
    piksi_log(LOG_ERR, "failed to create timer");
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  if (router_attach(router, loop) != 0) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  pk_loop_run_simple(loop);

  exit(cleanup(EXIT_SUCCESS, &loop, &router, &router_metrics));
}

static int cleanup(int result,
                   pk_loop_t **loop_loc,
                   router_t **router_loc,
                   pk_metrics_t **metrics_loc)
{
  router_teardown(router_loc);
  pk_loop_destroy(loop_loc);
  pk_metrics_destroy(metrics_loc);
  logging_deinit();
  return result;
}
