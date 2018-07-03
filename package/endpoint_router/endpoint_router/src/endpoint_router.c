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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>

#include <libpiksi/loop.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "endpoint_router.h"
#include "endpoint_router_load.h"
#include "endpoint_router_print.h"

#define PROGRAM_NAME "router"

static struct {
  const char *filename;
  bool print;
  bool debug;
} options = {
  .filename = NULL,
  .print = false,
  .debug = false
};

static void loop_reader_callback(pk_loop_t *loop, void *handle, void *context);

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("-f, --file <config.yml>");
  puts("--print");
  puts("--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_PRINT = 1,
    OPT_ID_DEBUG
  };

  const struct option long_opts[] = {
    {"file",      required_argument, 0, 'f'},
    {"print",     no_argument,       0, OPT_ID_PRINT},
    {"debug",     no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0}
  };

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "f:",
                          long_opts, &opt_index)) != -1) {
    switch (c) {

      case 'f': {
        options.filename = optarg;
      }
      break;

      case OPT_ID_PRINT: {
        options.print = true;
      }
      break;

      case OPT_ID_DEBUG: {
        options.debug = true;
      }
      break;

      default: {
        printf("invalid option\n");
        return -1;
      }
      break;
    }
  }

  if (options.filename == NULL) {
    printf("config file not specified\n");
    return -1;
  }

  return 0;
}

static int router_create_endpoints(router_cfg_t *router)
{
  port_t *port;

  for (port = router->ports_list; port != NULL; port = port->next) {

    port->pub_ept = pk_endpoint_create(port->pub_addr, PK_ENDPOINT_PUB_SERVER);
    if (port->pub_ept == NULL) {
      piksi_log(LOG_ERR, "pk_endpoint_create() error\n");
      return -1;
    }

    port->sub_ept = pk_endpoint_create(port->sub_addr, PK_ENDPOINT_SUB_SERVER);
    if (port->sub_ept == NULL) {
      piksi_log(LOG_ERR, "pk_endpoint_create() error\n");
      return -1;
    }
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

static void filter_match_process(forwarding_rule_t *forwarding_rule,
                                 filter_t *filter,
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
    }
    break;
  }
}

static void rule_process(forwarding_rule_t *forwarding_rule,
                         const u8 *data,
                         size_t length,
                         match_fn_t match_fn,
                         void *context)
{
  /* Iterate over filters for this rule */
  filter_t *filter;
  for (filter = forwarding_rule->filters_list; filter != NULL;
       filter = filter->next) {

    bool match = false;

    /* Empty filter matches all */
    if (filter->len == 0) {
      match = true;
    } else if (data != NULL) {
      if ((length >= filter->len) &&
          (memcmp(data, filter->data, filter->len) == 0)) {
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

static int reader_fn(const u8 *data, const size_t length, void *context)
{
  port_t *port = (port_t *)context;
  //rule_cache_t *rule_cache = (rule_cache_t *)context;

  // Steps for optimization
  // 1.) Hash incoming data according incoming lengths (all are 3 by default now)
  // 2.) Check map for match
  // 3.) If there's a match then forwarding to all ports in the map
  //    a.) if there's a match then send to all listed ports
  //    b.) if no match, send to all ports that are default accept

  // TODO: use rule_cache_t instead of:
  process_forwarding_rules(port->forwarding_rules_list, data, length, filter_match_process, NULL);

  return 0;
}

static void loop_reader_callback(pk_loop_t *loop, void *handle, void *context)
{
  (void)loop;
  (void)handle;
  port_t *port = (port_t *)context;

  pk_endpoint_receive(port->sub_ept, reader_fn, port);
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

void process_forwarding_rules(forwarding_rule_t *forwarding_rule,
                              const u8 *data,
                              const size_t length,
                              match_fn_t match_fn,
                              void* context)
{
  for (/* empty */; forwarding_rule != NULL; forwarding_rule = forwarding_rule->next) {
    rule_process(forwarding_rule, data, length, match_fn, context);
  }
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

  if (router_cfg == NULL)
    return NULL;

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

static int cleanup(int result, pk_loop_t **loop_loc, router_t **router_loc);

int main(int argc, char *argv[])
{
  pk_loop_t *loop = NULL;
  router_t *router = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }

  /* Load router from config file */
  router = router_create(options.filename);
  if (router == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }

  /* Print router config and exit if requested */
  if (options.print) {
    if (router_print(stdout, router->router_cfg) != 0) {
      exit(EXIT_FAILURE);
    }
    exit(cleanup(EXIT_SUCCESS, &loop, &router));
  }

  /* Set up router endpoints */
  if (router_create_endpoints(router->router_cfg) != 0) {
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }

  /* Create loop */
  loop = pk_loop_create();
  if (loop == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }

  /* Add router to loop */
  if (router_attach(router->router_cfg, loop) != 0) {
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }

  pk_loop_run_simple(loop);

  exit(cleanup(EXIT_SUCCESS, &loop, &router));
}

static int cleanup(int result, pk_loop_t **loop_loc, router_t **router_loc)
{
  router_teardown(router_loc);
  pk_loop_destroy(loop_loc);
  logging_deinit();

  return result;
}
