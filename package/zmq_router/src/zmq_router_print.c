/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "zmq_router_print.h"

#define PREFIX_STRING_SIZE_MAX 32

static int print_filter(FILE *f, const char *prefix, const filter_t *filter)
{
  const char *filter_action_str = filter->action == FILTER_ACTION_ACCEPT ?
                                  "ACCEPT" : "REJECT";
  fprintf(f, "%s%s ", prefix, filter_action_str);

  int i;
  for (i = 0; i < filter->len; i++) {
    fprintf(f, "0x%02X ", filter->data[i]);
  }

  fprintf(f, "\n");
  return 0;
}


static int print_forwarding_rule(FILE *f, const char *prefix,
                                 const forwarding_rule_t *forwarding_rule)
{
  fprintf(f, "%sdst_port: %s\n", prefix, forwarding_rule->dst_port_name);
  fprintf(f, "%sfilters:\n", prefix);

  char prefix_new[PREFIX_STRING_SIZE_MAX];
  snprintf(prefix_new, sizeof(prefix_new), "%s\t", prefix);

  const filter_t *filter;
  for (filter = forwarding_rule->filters_list; filter != NULL;
       filter = filter->next) {
    if (print_filter(f, prefix_new, filter) != 0) {
      return -1;
    }
  }
  return 0;
}

static int print_port(FILE *f, const char *prefix, const port_t *port)
{
  fprintf(f, "%s%s\n", prefix, port->name);
  fprintf(f, "%s\tpub_addr: %s\n", prefix, port->pub_addr);
  fprintf(f, "%s\tsub_addr: %s\n", prefix, port->sub_addr);
  fprintf(f, "%s\tforwarding_rules:\n", prefix);

  char prefix_new[PREFIX_STRING_SIZE_MAX];
  snprintf(prefix_new, sizeof(prefix_new), "%s\t\t", prefix);

  const forwarding_rule_t *forwarding_rule;
  for (forwarding_rule = port->forwarding_rules_list; forwarding_rule != NULL;
       forwarding_rule = forwarding_rule->next) {
    if (print_forwarding_rule(f, prefix_new, forwarding_rule) != 0) {
      return -1;
    }
  }
  return 0;
}

int zmq_router_print(FILE *f, const router_t *router)
{
  const char *prefix = "";

  fprintf(f, "%s%s\n", prefix, router->name);

  char prefix_new[PREFIX_STRING_SIZE_MAX];
  snprintf(prefix_new, sizeof(prefix_new), "%s\t", prefix);

  const port_t *port;
  for (port = router->ports_list; port != NULL; port = port->next) {
    if (print_port(f, prefix_new, port) != 0) {
      return -1;
    }
  }
  return 0;
}
