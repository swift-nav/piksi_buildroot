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

void rule_process(forwarding_rule_t *forwarding_rule, const u8 *data, size_t length, match_fn_t match_fn)
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
      match_fn(forwarding_rule, filter, data, length);

      /* Done with this rule after finding a filter match */
      break;
    }
  }
}

void process_forwarding_rules(forwarding_rule_t *forwarding_rule, const u8 *data, const size_t length, match_fn_t match_fn)
{
  for ( /*empty */; forwarding_rule != NULL; forwarding_rule = forwarding_rule->next) {
    rule_process(forwarding_rule, data, length, match_fn);
  }
}

static int reader_fn(const u8 *data, const size_t length, void *context)
{
  port_t *port = (port_t *)context;
  process_forwarding_rules(port->forwarding_rules_list, data, length, filter_match_process);

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
  router = router_load(options.filename);
  if (router == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }

  /* Print router config and exit if requested */
  if (options.print) {
    if (router_print(stdout, router) != 0) {
      exit(EXIT_FAILURE);
    }
    exit(cleanup(EXIT_SUCCESS, &loop, &router));
  }

  /* Set up router data */
  if (router_setup(router) != 0) {
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }

  /* Create loop */
  loop = pk_loop_create();
  if (loop == NULL) {
    exit(cleanup(EXIT_FAILURE, &loop, &router));
  }

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
