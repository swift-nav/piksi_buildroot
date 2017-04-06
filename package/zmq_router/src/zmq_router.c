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

#include "zmq_router.h"
#include "zmq_router_load.h"
#include "zmq_router_print.h"
#include <stdio.h>
#include <string.h>
#include <getopt.h>

static struct {
  const char *filename;
  bool print;
  bool debug;
} options = {
  .filename = NULL,
  .print = false,
  .debug = false
};

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
    port->pub_socket = zsock_new_pub(port->pub_addr);
    if (port->pub_socket == NULL) {
      printf("zsock_new_pub() error\n");
      return -1;
    }

    port->sub_socket = zsock_new_sub(port->sub_addr, "");
    if (port->sub_socket == NULL) {
      printf("zsock_new_sub() error\n");
      return -1;
    }
  }

  return 0;
}

static int zloop_router_add(zloop_t *zloop, router_t *router,
                            zloop_reader_fn reader_fn)
{
  port_t *port;
  for (port = router->ports_list; port != NULL; port = port->next) {
    if (zloop_reader(zloop, port->sub_socket, reader_fn, port) != 0) {
      printf("zloop_reader() error\n");
      return -1;
    }
  }

  return 0;
}

static void filter_match_process(forwarding_rule_t *forwarding_rule,
                                 filter_t *filter, zmsg_t *msg)
{
  switch (filter->action) {
  case FILTER_ACTION_ACCEPT: {
    zmsg_t *tx_msg = zmsg_dup(msg);
    if (tx_msg == NULL) {
      printf("zmsg_dup() error\n");
      break;
    }

    if (zmsg_send(&tx_msg, forwarding_rule->dst_port->pub_socket) != 0) {
      printf("zmsg_send() error\n");
      break;
    }
  }
  break;

  case FILTER_ACTION_REJECT: {

  }
  break;

  default: {
    printf("invalid filter action\n");
  }
  break;
}
}

static void rule_process(forwarding_rule_t *forwarding_rule,
                         const void *prefix, int prefix_len, zmsg_t *msg)
{
  /* Iterate over filters for this rule */
  filter_t *filter;
  for (filter = forwarding_rule->filters_list; filter != NULL;
       filter = filter->next) {

    bool match = false;

    /* Empty filter matches all */
    if (filter->len == 0) {
      match = true;
    } else if (prefix != NULL) {
      if ((prefix_len >= filter->len) &&
          (memcmp(prefix, filter->data, filter->len) == 0)) {
        match = true;
      }
    }

    if (match) {
      filter_match_process(forwarding_rule, filter, msg);

      /* Done with this rule after finding a filter match */
      break;
    }
  }
}

static int reader_fn(zloop_t *zloop, zsock_t *zsock, void *arg)
{
  port_t *port = (port_t *)arg;

  zmsg_t *rx_msg = zmsg_recv(port->sub_socket);
  if (rx_msg == NULL) {
    printf("zmsg_recv() error\n");
    return 0;
  }

  /* Get first frame for filtering */
  zframe_t *rx_frame_first = zmsg_first(rx_msg);
  const void *rx_prefix = NULL;
  size_t rx_prefix_len = 0;
  if (rx_frame_first != NULL) {
    rx_prefix = zframe_data(rx_frame_first);
    rx_prefix_len = zframe_size(rx_frame_first);
  }

  /* Iterate over forwarding rules */
  forwarding_rule_t *forwarding_rule;
  for (forwarding_rule = port->forwarding_rules_list; forwarding_rule != NULL;
       forwarding_rule = forwarding_rule->next) {
    rule_process(forwarding_rule, rx_prefix, rx_prefix_len, rx_msg);
  }

  zmsg_destroy(&rx_msg);
  return 0;
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

int main(int argc, char *argv[])
{
  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  /* Load router from config file */
  router_t *router = zmq_router_load(options.filename);
  if (router == NULL) {
    exit(EXIT_FAILURE);
  }

  /* Print router config and exit if requested */
  if (options.print) {
    if (zmq_router_print(stdout, router) != 0) {
      exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
  }

  /* Set up router data */
  if (router_setup(router) != 0) {
    exit(EXIT_FAILURE);
  }

  /* Create zloop */
  zloop_t *zloop = zloop_new();
  if (zloop == NULL) {
    exit(EXIT_FAILURE);
  }

  /* Add router to zloop */
  if (zloop_router_add(zloop, router, reader_fn) != 0) {
    exit(EXIT_FAILURE);
  }

  zloop_start(zloop);
  exit(EXIT_SUCCESS);
}
