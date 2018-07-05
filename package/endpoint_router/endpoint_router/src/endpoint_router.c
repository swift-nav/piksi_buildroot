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

#define NETWORK_BUF_SIZE 4096

static struct {
  const char *filename;
  bool print;
  bool debug;
} options = {
  .filename = NULL,
  .print = false,
  .debug = false
};

endpoint_send_fn_t endpoint_send_fn;

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

static int router_attach(router_t *router, pk_loop_t *loop)
{
  size_t idx = 0;
  port_t *port;

  for (port = router->router_cfg->ports_list; port != NULL; port = port->next, idx++) {

    // TODO: fork here for parallelization
#define SEND_FLUSH_MS 2
#define SEND_BUF_SIZE 4096
    //pk_endpoint_buffer_sends(port->pub_ept, loop, SEND_FLUSH_MS, SEND_BUF_SIZE);

    if (pk_loop_endpoint_reader_add(loop, port->sub_ept, loop_reader_callback, &router->port_rule_cache[idx]) == NULL) {
      piksi_log(LOG_ERR, "pk_loop_endpoint_reader_add() error\n");
      return -1;
    }
  }

  return 0;
}

static void cache_match_process(forwarding_rule_t *forwarding_rule,
                                 filter_t *filter,
                                 const u8 *data,
                                 size_t length,
                                 void *context)
{
  rule_cache_t *rule_cache = (rule_cache_t *) context;

  assert( length >= rule_cache->rule_prefixes->prefix_len );

  switch (filter->action) {
    case FILTER_ACTION_ACCEPT: {

      if (rule_cache->hash != NULL) {

        uint32_t key = cmph_search(rule_cache->hash, (const char *)data, rule_cache->rule_prefixes->prefix_len);
        rule_cache->cached_ports[key].endpoints[rule_cache->cached_ports[key].count++] = forwarding_rule->dst_port->pub_ept;
        memcpy(rule_cache->cached_ports[key].prefix, data, rule_cache->rule_prefixes->prefix_len);

      } else {
        // Port only has one accept rule, no need for a hash, incoming packets should
        //   get picked up by the rule_cache_t->accept_ports list
      }

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

int router_reader(const u8 *data, const size_t length, void *context)
{
  //fprintf(stderr, "%s: enter (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);
  rule_cache_t *rule_cache = (rule_cache_t *)context;

  //if (nbuf->size < rule_cache->rule_prefixes->prefix_len) {
  if (length < rule_cache->rule_prefixes->prefix_len) {
    // No match, send to all default accept ports
    for (size_t idx = 0; idx < rule_cache->accept_ports_count; idx++) {
      //pk_nbuf_t* nbuf_copy = pk_dupe_nbuf(nbuf);
      //fprintf(stderr, "%s: call out (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);
      //endpoint_send_fn(rule_cache->accept_ports[idx], &nbuf_copy);
      endpoint_send_fn(rule_cache->accept_ports[idx], data, length);
    }

    //fprintf(stderr, "%s: exit (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);
    return 0;
  }

  size_t prefix_len = rule_cache->rule_prefixes->prefix_len;
  //uint32_t key = cmph_search(rule_cache->hash, nbuf->buf, prefix_len);
  uint32_t key = cmph_search(rule_cache->hash, (const char *)data, prefix_len);

  //if (memcmp(rule_cache->cached_ports[key].prefix, nbuf->buf, prefix_len) == 0) {
  if (memcmp(rule_cache->cached_ports[key].prefix, data, prefix_len) == 0) {
    // Match, forward to list of rules
    for (size_t idx = 0; idx < rule_cache->cached_ports[key].count; idx++) {
      //pk_nbuf_t* nbuf_copy = pk_dupe_nbuf(nbuf);
      //fprintf(stderr, "%s: call out (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);
      //endpoint_send_fn(rule_cache->cached_ports[key].endpoints[idx], &nbuf_copy);
      endpoint_send_fn(rule_cache->cached_ports[key].endpoints[idx], data, length);
    }
  } else {
    // No match, forward to everything that's default accept
    for (size_t idx = 0; idx < rule_cache->accept_ports_count; idx++) {
      //pk_nbuf_t* nbuf_copy = pk_dupe_nbuf(nbuf);
      //fprintf(stderr, "%s: call out (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);
      //endpoint_send_fn(rule_cache->accept_ports[idx], &nbuf_copy);
      endpoint_send_fn(rule_cache->accept_ports[idx], data, length);
    }
  }

  //fprintf(stderr, "%s: exit (%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);
  return 0;
}

static void loop_reader_callback(pk_loop_t *loop, void *handle, void *context)
{
  (void)loop;
  (void)handle;

  rule_cache_t *rule_cache = (rule_cache_t *)context;

  //pk_endpoint_receive_nbuf(rule_cache->sub_ept, router_reader, rule_cache);
  pk_endpoint_receive(rule_cache->sub_ept, router_reader, rule_cache);
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

#define MSG_PREFIX_LEN_MISMATCH \
  "ERROR: all forwarding rule prefixes for a port must be the same length (%d vs %zu)\n"

#define MSG_PREFIX_LEN_MAX \
  "ERROR: forwarding rule prefix length (%d) exceeded maximum length (%d)\n"

rule_prefixes_t* extract_rule_prefixes(port_t *port, rule_cache_t *rule_cache)
{
  size_t total_filter_prefixes = 0;
  forwarding_rule_t *rules = NULL;

  int prefix_len = -1;

  for (rules = port->forwarding_rules_list; rules != NULL; rules = rules->next) {

    size_t filter_prefix_count = 0;
    filter_t *filter_last = NULL;

    for (filter_t *filter = rules->filters_list; filter != NULL; filter = filter->next) {

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

      if (filter->next == NULL) {
        filter_last = filter;
      }
    }

    total_filter_prefixes += filter_prefix_count;

    // Check if this is a "default accept" filter chain
    if (filter_last->action == FILTER_ACTION_ACCEPT) {
      rule_cache->accept_ports[rule_cache->accept_ports_count++] = rules->dst_port->pub_ept;
    }
  }

  size_t filter_prefix_index = 0;
  u8 (*all_filter_prefixes)[MAX_PREFIX_LEN] = calloc(total_filter_prefixes, sizeof(u8) * MAX_PREFIX_LEN);

  STAGE_CLEANUP(all_filter_prefixes, ({ free(all_filter_prefixes); }));

  for (rules = port->forwarding_rules_list; rules != NULL; rules = rules->next) {
    for (filter_t *filter = rules->filters_list; filter != NULL; filter = filter->next) {
      if (filter->data != NULL && filter->len > 0) {
        u8* filter_prefix = all_filter_prefixes[filter_prefix_index++];
        memcpy(filter_prefix, filter->data, prefix_len);
      }
    }
  }

  assert(total_filter_prefixes == filter_prefix_index);

  int compare_prefixes(const void *a, const void *b) {
    return memcmp(a, b, (size_t)prefix_len);
  };

  qsort(all_filter_prefixes, total_filter_prefixes, MAX_PREFIX_LEN, compare_prefixes);

  u8 (*deduped_filter_prefixes)[MAX_PREFIX_LEN] = calloc(total_filter_prefixes, sizeof(u8) * MAX_PREFIX_LEN);

  STAGE_CLEANUP(deduped_filter_prefixes, ({
    if (deduped_filter_prefixes != NULL ) free(deduped_filter_prefixes);
  }));

  size_t deduped_filter_index = 0;

  u8* prev_entry = NULL;
  for (size_t filter_idx = 0; filter_idx < total_filter_prefixes; filter_idx++) {
    if (prev_entry != NULL && memcmp(prev_entry, all_filter_prefixes[filter_idx], prefix_len) == 0) {
      continue;
    }
    prev_entry = all_filter_prefixes[filter_idx];
    memcpy(deduped_filter_prefixes[deduped_filter_index++], all_filter_prefixes[filter_idx], prefix_len);
  }

  rule_prefixes_t *rule_prefixes = malloc(sizeof(rule_prefixes_t));
  STAGE_CLEANUP(rule_prefixes, ({if (rule_prefixes != NULL) free(rule_prefixes);}));

  rule_prefixes->prefixes = UNSTAGE_CLEANUP_X(deduped_filter_prefixes, ARRAY_OF_FIXED_BLOCKS(u8, MAX_PREFIX_LEN));
  rule_prefixes->count = deduped_filter_index;
  rule_prefixes->prefix_len = prefix_len;

  return UNSTAGE_CLEANUP(rule_prefixes, rule_prefixes_t*);
}

router_t* router_create(const char *filename, load_endpoints_fn_t load_endpoints)
{
  router_t *router = malloc(sizeof(router_t));
  STAGE_CLEANUP(router, ({ if (router != NULL) free(router); }));

  router_cfg_t *router_cfg = router_cfg_load(filename);

  if (router_cfg == NULL) {
    return NULL;
  }

  STAGE_CLEANUP(router_cfg, ({
    if (router_cfg != NULL) router_cfg_teardown(&router_cfg);
  }));

  if (load_endpoints(router_cfg) != 0) {
    return NULL;
  }

  router->router_cfg = UNSTAGE_CLEANUP(router_cfg, router_cfg_t*);

  router->port_count = 0;
  for (port_t *port = router->router_cfg->ports_list; port != NULL; port = port->next) {
    router->port_count++;
  }

  router->port_rule_cache = calloc(router->port_count, sizeof(rule_cache_t));

  size_t port_index = 0;

  for (port_t *port = router->router_cfg->ports_list; port != NULL; port = port->next) {

    rule_cache_t *rule_cache = &router->port_rule_cache[port_index];

    rule_cache->router = router;
    rule_cache->sub_ept = port->sub_ept;
    rule_cache->rule_count = 0;

    forwarding_rule_t *rules = port->forwarding_rules_list;

    for (/* empty */; rules != NULL; rules = rules->next) {
      rule_cache->rule_count++;
    }

    rule_cache->accept_ports = calloc(rule_cache->rule_count, sizeof(pk_endpoint_t*));

    rule_prefixes_t *rule_prefixes = extract_rule_prefixes(port, rule_cache);

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

      for (size_t idx = 0; idx < rule_prefixes->count; idx++) {
        rule_cache->cached_ports[idx].endpoints = calloc(router->port_count, sizeof(pk_endpoint_t*));
        rule_cache->cached_ports[idx].count = 0;
      }

      for (size_t idx = 0; idx < rule_prefixes->count; idx++) {
#ifdef DEBUG_ENDPOINT_ROUTER
        u8 test[] = { 0x55, 0xaf, 0x00 };
        uint32_t key = cmph_search(rule_cache->hash, (char*)test, 3);
        fprintf(stderr, "[%zu]: %02x %02x %02x, len=%zu, key=%d\n",
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

  return UNSTAGE_CLEANUP(router, router_t*);
}

void router_teardown(router_t **router_loc)
{
  if (*router_loc == NULL) return;

  router_t *router = *router_loc;
  router_cfg_teardown(&router->router_cfg);

  for (size_t rule_idx = 0; rule_idx < router->port_count; rule_idx++) {

    rule_cache_t* rule_cache = &router->port_rule_cache[rule_idx];

    if (rule_cache->cached_ports != NULL) {

      for (size_t idx = 0; idx < rule_cache->rule_prefixes->count; idx++) {
        if (rule_cache->cached_ports->endpoints != NULL) {
          free(rule_cache->cached_ports[idx].endpoints);
        }
      }

      free(rule_cache->cached_ports);
    }

    if (rule_cache->accept_ports != NULL) {
      free(rule_cache->accept_ports);
    }

    rule_prefixes_destroy(&rule_cache->rule_prefixes);

    if (rule_cache->hash != NULL){
      cmph_destroy(rule_cache->hash);
    }

    if (rule_cache->cmph_io_adapter != NULL){
      cmph_io_struct_vector_adapter_destroy(rule_cache->cmph_io_adapter);
    }
  }

  free(router->port_rule_cache);
  free(router);

  *router_loc = NULL;
}

static int cleanup(int result, pk_loop_t **loop_loc, router_t **router_loc);

#if 0
static int ipc_sock;

void fatal(const char *func)
{
  fprintf(stderr, "%s: %s\n", func, nn_strerror(nn_errno()));
  exit(1);
}

void ipc_port_cb(pk_loop_t *loop, void *handle, void *context)
{
  char *buf = NULL;
  int bytes;

  if ((bytes = nn_recv(sock, &buf, NN_MSG, 0)) < 0) {
    fatal("nn_recv");
  }
}
#endif

int main(int argc, char *argv[])
{
  pk_loop_t *loop = NULL;
  router_t *router = NULL;

  endpoint_destroy_fn = pk_endpoint_destroy;
  //endpoint_send_fn = pk_endpoint_send_nbuf;
  endpoint_send_fn = pk_endpoint_send;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }

  /* Load router from config file */
  router = router_create(options.filename, router_create_endpoints);
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
#if 0
  if ((ipc_sock = nn_socket (AF_SP, NN_REP)) < 0) {
    fatal("nn_socket");
  }

  if (nn_bind(sock, "ipc:///tmp/tmp.endpoint_router") < 0) {
    fatal("nn_bind");
  }
#endif
  /* Create loop */
  loop = pk_loop_create();
  if (loop == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }
#if 0
  pk_loop_poll_add(loop, sock, ipc_loop_cb, router);
#endif
  /* Add router to loop */
  if (router_attach(router, loop) != 0) {
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
