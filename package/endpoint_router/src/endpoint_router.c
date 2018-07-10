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
#include <libpiksi/metrics.h>
#include <libpiksi/util.h>

#include "endpoint_router.h"
#include "endpoint_router_load.h"
#include "endpoint_router_print.h"

#define PROGRAM_NAME "router"

static struct {
  const char *filename;
  const char *name;
  bool print;
  bool debug;
} options = {
  .filename = NULL,
  .name = NULL,
  .print = false,
  .debug = false
};

static struct {
  ssize_t count_snapshot;
  ssize_t count;
  ssize_t size_snapshot;
  ssize_t size;
} message_metrics = {
  .count_snapshot = -1,
  .count = -1,
  .size_snapshot = -1,
  .size = -1,
};

static pk_metrics_t *router_metrics = NULL;

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
    OPT_ID_NAME,
    OPT_ID_DEBUG
  };

  const struct option long_opts[] = {
    {"file",      required_argument, 0, 'f'},
    {"name",      required_argument, 0, OPT_ID_NAME},
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

      case OPT_ID_NAME: {
        options.name = optarg;
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

static void filters_destroy(filter_t **filter_loc)
{
  if (filter_loc == NULL || *filter_loc == NULL) {
    return;
  }
  filter_t *filter = *filter_loc;
  filter_t *next = NULL;
  while(filter != NULL) {
    next = filter->next;
    if (filter->data != NULL) free(filter->data);
    free(filter);
    filter = next;
  }
  *filter_loc = NULL;
}

static void forwarding_rules_destroy(forwarding_rule_t **forwarding_rule_loc)
{
  if (forwarding_rule_loc == NULL || *forwarding_rule_loc == NULL) {
    return;
  }
  forwarding_rule_t *forwarding_rule = *forwarding_rule_loc;
  forwarding_rule_t *next = NULL;
  while(forwarding_rule != NULL) {
    next = forwarding_rule->next;
    if (forwarding_rule->dst_port_name != NULL
        && forwarding_rule->dst_port_name[0] != '\0') {
      free((void *)forwarding_rule->dst_port_name);
    }
    filters_destroy(&forwarding_rule->filters_list);
    free(forwarding_rule);
    forwarding_rule = next;
  }
  *forwarding_rule_loc = NULL;
}

static void ports_destroy(port_t **port_loc)
{
  if (port_loc == NULL || *port_loc == NULL) {
    return;
  }
  port_t *port = *port_loc;
  port_t *next = NULL;
  while(port != NULL) {
    next = port->next;
    if (port->name != NULL && port->name[0] != '\0') {
      free((void *)port->name);
    }
    if (port->pub_addr != NULL && port->pub_addr[0] != '\0') {
      free((void *)port->pub_addr);
    }
    if (port->sub_addr != NULL && port->sub_addr[0] != '\0') {
      free((void *)port->sub_addr);
    }
    pk_endpoint_destroy(&port->pub_ept);
    pk_endpoint_destroy(&port->sub_ept);
    forwarding_rules_destroy(&port->forwarding_rules_list);
    free(port);
    port = next;
  }
  free(port);
  *port_loc = NULL;
}

static void router_teardown(router_t **router_loc)
{
  if (router_loc == NULL || *router_loc == NULL) {
    return;
  }
  router_t *router = *router_loc;
  if (router->name != NULL) free((void *)router->name);
  ports_destroy(&router->ports_list);
  free(router);
  *router_loc = NULL;
}

static int router_setup(router_t *router)
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
  port_t *port;
  for (port = router->ports_list; port != NULL; port = port->next) {
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
                                 size_t length)
{
  switch (filter->action) {
    case FILTER_ACTION_ACCEPT: {
      pk_endpoint_send(forwarding_rule->dst_port->pub_ept, data, length);
    }
    break;

    case FILTER_ACTION_REJECT: {

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
                         size_t length)
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
      filter_match_process(forwarding_rule, filter, data, length);

      /* Done with this rule after finding a filter match */
      break;
    }
  }
}

static int reader_fn(const u8 *data, const size_t length, void *context)
{
  port_t *port = (port_t *)context;

  pk_metrics_update(router_metrics, message_metrics.count_snapshot, (pk_metrics_value_t) { .u32 = 1 }, NULL);
  pk_metrics_update(router_metrics, message_metrics.size_snapshot, (pk_metrics_value_t) { .u32 = (u32)length }, NULL);

  /* Iterate over forwarding rules */
  forwarding_rule_t *forwarding_rule;
  for (forwarding_rule = port->forwarding_rules_list; forwarding_rule != NULL;
       forwarding_rule = forwarding_rule->next) {
    rule_process(forwarding_rule, data, length);
  }

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

static int cleanup(int result, pk_loop_t **loop_loc, router_t **router_loc, pk_metrics_t **metrics);

static void loop_1s_metrics(pk_loop_t *loop, void *handle, void *context)
{
  pk_metrics_value_t count;
  pk_metrics_value_t size;

  pk_metrics_read(router_metrics, message_metrics.count_snapshot, &count);
  pk_metrics_update(router_metrics, message_metrics.count, count, NULL);

  pk_metrics_read(router_metrics, message_metrics.size_snapshot, &size);
  pk_metrics_update(router_metrics, message_metrics.size,
                    (pk_metrics_value_t) { .u32 = count.u32 == 0 ? 0 : size.u32 / count.u32 },
                    NULL);

  pk_metrics_flush(router_metrics);

  pk_metrics_reset(router_metrics, message_metrics.count_snapshot);
  pk_metrics_reset(router_metrics, message_metrics.size_snapshot);

  pk_loop_timer_reset(handle);
}

static bool setup_metrics()
{
  size_t count = 0;
  char metrics_folder[128] = {0};
  const char* metrics_name = NULL;
  ssize_t metrics_index = -1;

  router_metrics = pk_metrics_create();

  if (router_metrics == NULL) {
    piksi_log(LOG_ERR, "metrics create failed");
    return false;
  }

  struct {
    const char* metrics_name;
    const char* metrics_folder;
    pk_metrics_updater_fn_t updater;
    ssize_t *metrics_index;
  }
    metrics_table[] =
  {
    { .metrics_name = "snapshot",   .metrics_folder = "message_count", .updater = pk_metrics_updater_sum,    &message_metrics.count_snapshot },
    { .metrics_name = "per_second", .metrics_folder = "message_count", .updater = pk_metrics_updater_assign, &message_metrics.count },
    { .metrics_name = "snapshot",   .metrics_folder = "message_size",  .updater = pk_metrics_updater_sum,    &message_metrics.size_snapshot },
    { .metrics_name = "per_second", .metrics_folder = "message_size",  .updater = pk_metrics_updater_assign, &message_metrics.size },
  };

  for (size_t idx = 0; idx < COUNT_OF(metrics_table); idx++) {

    count = snprintf(metrics_folder, sizeof(metrics_folder), "endpoint_router_%s/%s", options.name, metrics_table[idx].metrics_folder);
    assert( count < sizeof(metrics_folder) );

    metrics_name = metrics_table[idx].metrics_name;

    metrics_index =
      pk_metrics_add(router_metrics,
                     metrics_folder,
                     metrics_name,
                     METRICS_TYPE_U32,
                     (pk_metrics_value_t) { .u32 = 0 },
                     metrics_table[idx].updater);

    if (metrics_index < 0) {
      goto fail;
    }

    *metrics_table[idx].metrics_index = metrics_index;
  }

  return true;

fail:
  piksi_log(LOG_ERR, "metrics add for '%s/%s' failed: %s",
            metrics_folder,
            metrics_name,
            pk_metrics_status_text(metrics_index));

  return false;
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

  /* Load router from config file */
  router = router_load(options.filename);
  if (router == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  /* Print router config and exit if requested */
  if (options.print) {
    if (router_print(stdout, router) != 0) {
      exit(EXIT_FAILURE);
    }
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  /* Set up router data */
  if (router_setup(router) != 0) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  /* Create loop */
  loop = pk_loop_create();
  if (loop == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  if (!setup_metrics()) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  void *handle =
    pk_loop_timer_add(loop,
                      1000,
                      loop_1s_metrics,
                      NULL);

  assert( handle != NULL );

  /* Add router to loop */
  if (router_attach(router, loop) != 0) {
    exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
  }

  pk_loop_run_simple(loop);

  exit(cleanup(EXIT_FAILURE, &loop, &router, &router_metrics));
}

static int cleanup(int result, pk_loop_t **loop_loc, router_t **router_loc, pk_metrics_t **metrics_loc)
{
  router_teardown(router_loc);
  pk_loop_destroy(loop_loc);
  pk_metrics_destory(metrics_loc);
  logging_deinit();
  return result;
}
