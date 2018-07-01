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

/* clang-format off */
PK_METRICS_TABLE(message_metrics_table, MI,

  PK_METRICS_ENTRY("message/count",    "per_second",  M_U32,   M_UPDATE_COUNT,   M_RESET_DEF,  count),
  PK_METRICS_ENTRY("message/size",     ".total",      M_U32,   M_UPDATE_SUM,     M_RESET_DEF,  size_total),
  PK_METRICS_ENTRY("message/size",     "per_second",  M_U32,   M_UPDATE_AVERAGE, M_RESET_DEF,  size,
                   M_AVERAGE_OF(MI,    size_total,    count)),
  PK_METRICS_ENTRY("message/wake_ups", "per_second",  M_U32,   M_UPDATE_COUNT,   M_RESET_DEF,  wakeups),
  PK_METRICS_ENTRY("message/wake_ups", "max",         M_U32,   M_UPDATE_MAX,     M_RESET_DEF,  wakeups_max),
  PK_METRICS_ENTRY("message/wake_ups", ".total",      M_U32,   M_UPDATE_COUNT,   M_RESET_DEF,  wakeups_message_count),
  PK_METRICS_ENTRY("message/latency",  "max",         M_TIME,  M_UPDATE_MAX,     M_RESET_DEF,  latency_max),
  PK_METRICS_ENTRY("message/latency",  ".delta",      M_TIME,  M_UPDATE_DELTA,   M_RESET_TIME, latency_delta),
  PK_METRICS_ENTRY("message/latency",  ".total",      M_TIME,  M_UPDATE_SUM,     M_RESET_DEF,  latency_total),
  PK_METRICS_ENTRY("message/latency",  "per_second",  M_TIME,  M_UPDATE_AVERAGE, M_RESET_DEF,  latency,
                   M_AVERAGE_OF(MI,    latency_total, count)),
  PK_METRICS_ENTRY("frame/count",      "per_second",  M_U32,   M_UPDATE_SUM,     M_RESET_DEF,  frame_count),
  PK_METRICS_ENTRY("frame/leftover",   "bytes",       M_U32,   M_UPDATE_SUM,     M_RESET_DEF,  frame_leftovers)
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

static pk_metrics_t *router_metrics = NULL;
static framer_t *framer_sbp = NULL;

static void loop_reader_callback(pk_loop_t *loop, void *handle, int status, void *context);
static void process_buffer(port_t *port, const u8 *data, const size_t length);
static void process_buffer_via_framer(port_t *port, const u8 *data, const size_t length);

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
      piksi_log(LOG_ERR, "pk_endpoint_create() error\n");
      return -1;
    }

    pk_endpoint_loop_add(port->pub_ept, loop);

    snprintf_assert(endpoint_metric, sizeof(endpoint_metric), "router/%s/sub_server", port->metric);

    port->sub_ept = pk_endpoint_create(pk_endpoint_config()
                                         .endpoint(port->sub_addr)
                                         .identity(endpoint_metric)
                                         .type(PK_ENDPOINT_SUB_SERVER)
                                         .get());
    if (port->sub_ept == NULL) {
      piksi_log(LOG_ERR, "pk_endpoint_create() error\n");
      return -1;
    }

    pk_endpoint_loop_add(port->sub_ept, loop);
  }

  return 0;
}

static int router_attach(router_cfg_t *router, pk_loop_t *loop)
{
  port_t *port;
  for (port = router->ports_list; port != NULL; port = port->next) {

    // TODO: fork here for parallelization

    if (pk_loop_endpoint_reader_add(loop, port->sub_ept, loop_reader_callback, port) == NULL) {
      piksi_log(LOG_ERR, "pk_loop_endpoint_reader_add() error\n");
      return -1;
    }
  }

  return 0;
}

static void filter_match_process(const forwarding_rule_t *forwarding_rule,
                                 const filter_t *filter,
                                 const u8 *data,
                                 size_t length,
                                 void *context)
{
  (void) context;

  //rule_cache_t *rule_cache = (rule_cache_t *) context;
  // TODO: ^^

  switch (filter->action) {
    case FILTER_ACTION_ACCEPT: {
      pk_endpoint_send(forwarding_rule->dst_port->pub_ept, data, length);
      // TODO: full out cache struct
    }
    break;

    case FILTER_ACTION_REJECT: {
      // NOOP
    }
    break;

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

static void process_buffer_via_framer(port_t *port, const u8 *data, const size_t length)
{
  size_t buffer_index = 0;

  u32 frame_count = 0;
  u32 leftover = 0;

  while (buffer_index < length) {

    const uint8_t *frame;
    uint32_t frame_length;

    buffer_index +=
      framer_process(framer_sbp, &data[buffer_index], length - buffer_index, &frame, &frame_length);

    if (frame == NULL) break;

    process_buffer(port, frame, frame_length);
    frame_count += 1;
  }

  leftover = length - buffer_index;

  PK_METRICS_UPDATE(MR, MI.frame_count, PK_METRICS_VALUE(frame_count));
  PK_METRICS_UPDATE(MR, MI.frame_leftovers, PK_METRICS_VALUE(leftover));
}

static void process_buffer(port_t *port, const u8 *data, const size_t length)
{
  process_forwarding_rules(port->forwarding_rules_list, data, length, filter_match_process);
}

void process_forwarding_rules(const forwarding_rule_t *forwarding_rule,
                              const u8 *data,
                              const size_t length,
                              match_fn_t match_fn)
{
  for (/*empty */; forwarding_rule != NULL; forwarding_rule = forwarding_rule->next) {
    process_forwarding_rule(forwarding_rule, data, length, match_fn);
  }
}

static int reader_fn(const u8 *data, const size_t length, void *context)
{
  port_t *port = (port_t *)context;

  PK_METRICS_UPDATE(router_metrics, MI.count);
  PK_METRICS_UPDATE(router_metrics, MI.wakeups_message_count);

  PK_METRICS_UPDATE(router_metrics, MI.size_total, PK_METRICS_VALUE((u32)length));

  if (options.process_sbp) {
    process_buffer_via_framer(port, data, length);
  } else {
    process_buffer(port, data, length);
  }

  return 0;
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

  port_t *port = (port_t *)context;

  pre_receive_metrics();
  pk_endpoint_receive(port->sub_ept, reader_fn, port);
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

  pk_loop_timer_reset(handle);
}

#define UNSTAGE_CLEANUP(TheReturn, TheType)                         \
  ({ TheType __unstage = TheReturn; TheReturn = NULL; __unstage; })

#define ARRAY_OF_FIXED_BLOCKS(TheType, TheSize) \
  TheType (*__unstage)[TheSize]

#define UNSTAGE_CLEANUP_X(TheReturn, TypeNameDecl)              \
  ({ TypeNameDecl = TheReturn; TheReturn = NULL; __unstage; })

#define STAGE_CLEANUP(TheVar, CleanUpExpr)                              \
  void clean_up_ ## TheVar (int* _X_ ## TheVar) {                       \
    (void) _X_ ## TheVar;                                               \
    CleanUpExpr;                                                        \
  };                                                                    \
  int _X_clean_up_ ## TheVar                                            \
    __attribute__((__cleanup__(clean_up_ ## TheVar))) = 0;              \
  (void) _X_clean_up_ ## TheVar;                                        \

rule_prefixes_t* extract_rule_prefixes(port_t *port)
{
  size_t total_filter_prefixes = 0;
  forwarding_rule_t *rules = NULL;

  int prefix_len = -1;

  for (rules = port->forwarding_rules_list; rules != NULL; rules = rules->next) {
    size_t filter_count = 0;
    for (filter_t *filter = rules->filters_list; filter != NULL; filter = filter->next) {
      filter_count++;
      if (prefix_len < 0) {
        prefix_len = filter->len;
      } else if (prefix_len != (int)filter->len) {
        // TODO: error case, fail
      }
      if (prefix_len > MAX_PREFIX_LEN) {
        // TOOD: error case, fail
      }
    }
    total_filter_prefixes += filter_count;
  }

  size_t filter_prefix_index = 0;
  size_t prefix_buf_len = sizeof(u8) * MAX_PREFIX_LEN * total_filter_prefixes;

  u8 (*all_filter_prefixes)[MAX_PREFIX_LEN] = malloc(prefix_buf_len);
  memset(all_filter_prefixes, 0, prefix_buf_len);

  STAGE_CLEANUP(all_filter_prefixes, ({free(all_filter_prefixes);}));

  for (rules = port->forwarding_rules_list; rules != NULL; rules = rules->next) {
    for (filter_t *filter = rules->filters_list; filter != NULL; filter = filter->next) {
      u8* filter_prefix = all_filter_prefixes[filter_prefix_index++];
      if (filter->data != NULL && filter->len > 0) {
        memcpy(filter_prefix, filter->data, prefix_len);
      }
    }
  }

  int compare_prefixes(const void *a, const void *b) {
    return memcmp(a, b, (size_t)prefix_len);
  };

  qsort(all_filter_prefixes, total_filter_prefixes, MAX_PREFIX_LEN, compare_prefixes);

  u8 (*deduped_filter_prefixes)[MAX_PREFIX_LEN] = malloc(prefix_buf_len);
  memset(deduped_filter_prefixes, 0, prefix_buf_len);

  STAGE_CLEANUP(deduped_filter_prefixes, ({
    if (deduped_filter_prefixes != NULL ) free(deduped_filter_prefixes);
  }));

  size_t deduped_filter_index = 0;

  u8* prev_entry = NULL;
  for (size_t filter_idx = 0; filter_idx < total_filter_prefixes; filter_idx++) {
    if (prev_entry == NULL) {
      prev_entry = all_filter_prefixes[filter_idx];
      continue;
    }
    if (memcmp(prev_entry, all_filter_prefixes[filter_idx], prefix_len) != 0) {
      memcpy(deduped_filter_prefixes[deduped_filter_index++], all_filter_prefixes[filter_idx], prefix_len);
    }
    prev_entry = all_filter_prefixes[filter_idx];
  }

  rule_prefixes_t *rule_prefixes = malloc(sizeof(rule_prefixes_t));
  STAGE_CLEANUP(rule_prefixes, ({if (rule_prefixes != NULL) free(rule_prefixes);}));

  rule_prefixes->prefixes = UNSTAGE_CLEANUP_X(deduped_filter_prefixes, ARRAY_OF_FIXED_BLOCKS(u8, MAX_PREFIX_LEN));
  rule_prefixes->count = deduped_filter_index;
  rule_prefixes->prefix_len = prefix_len;

  return UNSTAGE_CLEANUP(rule_prefixes, rule_prefixes_t*);
}

router_t* router_create(const char *filename)
{
  router_t *router = malloc(sizeof(router_t));
  STAGE_CLEANUP(router, ({
                  if (router != NULL) free(router);
                }));

  router_cfg_t *router_cfg = router_cfg_load(filename);

  if (router_cfg == NULL) return NULL;

  router->router_cfg = router_cfg;
  size_t port_count = 0;
  for (port_t *port = router_cfg->ports_list; port != NULL; port = port->next) {
    port_count++;
  }

  router->port_rule_cache = malloc(sizeof(rule_cache_t) * port_count);
  size_t port_index = 0;

  for (port_t *port = router_cfg->ports_list; port != NULL; port = port->next) {

    rule_cache_t *rule_cache = &router->port_rule_cache[port_index];

    size_t rule_count = 0;
    forwarding_rule_t *rules = port->forwarding_rules_list;
    for (/* empty */; rules != NULL; rules = rules->next) {
      rule_count++;
    }

    (void) rule_cache;
    /*
    rule_cache->accept_ports = malloc(sizeof(pk_endpoint_t*) * rule_count);
    rule_cache->default_accept_ports = malloc(sizeof(pk_endpoint_t*) * rule_count);
    */

    rule_prefixes_t *rule_prefixes = extract_rule_prefixes(port);
    STAGE_CLEANUP(rule_prefixes, ({
          if(rule_prefixes != NULL) rule_prefixes_destroy(&rule_prefixes);
        }));

    // TODO: Create cmph hash

    /*
      process_forwarding_rules(port->forwarding_rules_list,
      filter->data,
      filter->len, filter_match_process,
      rule_cache);
    */

    port_index++;
  }

  return UNSTAGE_CLEANUP(router, router_t*);
}

void router_teardown(router_t **router_loc)
{
  if (*router_loc == NULL) return;

  router_t *router = *router_loc;
  router_cfg_teardown(&router->router_cfg);

  /* TODO:
    rule_cache->accept_ports = malloc(sizeof(pk_endpoint_t*) * rule_count);
    rule_cache->default_accept_ports = malloc(sizeof(pk_endpoint_t*) * rule_count);
  */

  free(router->port_rule_cache);

  // TODO: Tear down cmph stuff
  free(router);

  *router_loc = NULL;
}

int main(int argc, char *argv[])
{
  pk_loop_t *loop = NULL;
  router_t *router = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  if (options.process_sbp) {

    const char *protocol_library_path = getenv(PROTOCOL_LIBRARY_PATH_ENV_NAME);
    if (protocol_library_path == NULL) {
      protocol_library_path = PROTOCOL_LIBRARY_PATH_DEFAULT;
    }

    if (protocols_import(protocol_library_path) != 0) {
      syslog(LOG_ERR, "error importing protocols");
      fprintf(stderr, "error importing protocols\n");
      exit(EXIT_FAILURE);
    }

    /* TODO: Clean-up */
    framer_sbp = framer_create("sbp");
    assert(framer_sbp != NULL);
  }

  /* Load router from config file */
  router = router_create(options.filename);
  if (router == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  /* Print router config and exit if requested */
  if (options.print) {
    if (router_print(stdout, router->router_cfg) != 0) {
      exit(EXIT_FAILURE);
    }
    exit(cleanup(EXIT_SUCCESS, &loop, &router, &router_metrics));
  }

  /* Create loop */
  loop = pk_loop_create();
  if (loop == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  /* Set up router endpoints */
  if (router_create_endpoints(router->router_cfg, loop) != 0) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  router_metrics = pk_metrics_setup("endpoint_router", options.name, MT, COUNT_OF(MT));
  if (router_metrics == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  void *handle = pk_loop_timer_add(loop, 1000, loop_1s_metrics, NULL);

  assert(handle != NULL);

  /* Add router to loop */
  if (router_attach(router->router_cfg, loop) != 0) {
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
