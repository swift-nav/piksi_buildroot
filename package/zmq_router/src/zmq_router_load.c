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

#include "zmq_router_load.h"
#include <yaml.h>

#define PROCESS_FN(name) int process_##name(yaml_event_t *event,              \
                                            yaml_parser_t *parser,            \
                                            void *context)

typedef struct {
  yaml_event_type_t event_type;
  const char *scalar_value;
  int (*process)(yaml_event_t *event, yaml_parser_t *parser, void *context);
  bool next;
} expected_event_t;

static PROCESS_FN(router);
static PROCESS_FN(router_name);
static PROCESS_FN(ports);
static PROCESS_FN(port);
static PROCESS_FN(port_name);
static PROCESS_FN(pub_addr);
static PROCESS_FN(sub_addr);
static PROCESS_FN(forwarding_rules);
static PROCESS_FN(forwarding_rule);
static PROCESS_FN(dst_port);
static PROCESS_FN(filters);
static PROCESS_FN(filter);
static PROCESS_FN(action);
static PROCESS_FN(prefix);
static PROCESS_FN(prefix_element);

static expected_event_t router_events[] = {
  { YAML_STREAM_START_EVENT, NULL, NULL, false },
  { YAML_DOCUMENT_START_EVENT, NULL, NULL, false },
  { YAML_MAPPING_START_EVENT, NULL, NULL, false },
  { YAML_SCALAR_EVENT, "name", process_router_name, true },
  { YAML_SCALAR_EVENT, "ports", process_ports, true },
  { YAML_MAPPING_END_EVENT, NULL, NULL, false },
  { YAML_DOCUMENT_END_EVENT, NULL, NULL, false },
  { YAML_STREAM_END_EVENT, NULL, NULL, false },
  { YAML_NO_EVENT, NULL, NULL, false }
};

static expected_event_t ports_events[] = {
  { YAML_SEQUENCE_START_EVENT, NULL, NULL, false },
  { YAML_MAPPING_START_EVENT, NULL, process_port, true },
  { YAML_SEQUENCE_END_EVENT, NULL, NULL, false },
  { YAML_NO_EVENT, NULL, NULL, false }
};

static expected_event_t port_events[] = {
  { YAML_SCALAR_EVENT, "name", process_port_name, true },
  { YAML_SCALAR_EVENT, "pub_addr", process_pub_addr, true },
  { YAML_SCALAR_EVENT, "sub_addr", process_sub_addr, true },
  { YAML_SCALAR_EVENT, "forwarding_rules", process_forwarding_rules, true },
  { YAML_MAPPING_END_EVENT, NULL, NULL, false },
  { YAML_NO_EVENT, NULL, NULL, false }
};

static expected_event_t forwarding_rules_events[] = {
  { YAML_SEQUENCE_START_EVENT, NULL, NULL, false },
  { YAML_MAPPING_START_EVENT, NULL, process_forwarding_rule, true },
  { YAML_SEQUENCE_END_EVENT, NULL, NULL, false },
  { YAML_NO_EVENT, NULL, NULL, false }
};

static expected_event_t forwarding_rule_events[] = {
  { YAML_SCALAR_EVENT, "dst_port", process_dst_port, true },
  { YAML_SCALAR_EVENT, "filters", process_filters, true },
  { YAML_MAPPING_END_EVENT, NULL, NULL, false },
  { YAML_NO_EVENT, NULL, NULL, false }
};

static expected_event_t filters_events[] = {
  { YAML_SEQUENCE_START_EVENT, NULL, NULL, false },
  { YAML_MAPPING_START_EVENT, NULL, process_filter, true },
  { YAML_SEQUENCE_END_EVENT, NULL, NULL, false },
  { YAML_NO_EVENT, NULL, NULL, false }
};

static expected_event_t filter_events[] = {
  { YAML_SCALAR_EVENT, "action", process_action, true },
  { YAML_SCALAR_EVENT, "prefix", process_prefix, true },
  { YAML_MAPPING_END_EVENT, NULL, NULL, false },
  { YAML_NO_EVENT, NULL, NULL, false }
};

static expected_event_t prefix_events[] = {
  { YAML_SEQUENCE_START_EVENT, NULL, NULL, false },
  { YAML_SCALAR_EVENT, NULL, process_prefix_element, true },
  { YAML_SEQUENCE_END_EVENT, NULL, NULL, false },
  { YAML_NO_EVENT, NULL, NULL, false }
};

static int event_scalar_value_get(yaml_parser_t *parser, char **str)
{
  yaml_event_t event;
  if (!yaml_parser_parse(parser, &event)) {
    printf("YAML parser error: %d\n", parser->error);
    return -1;
  }

  int ret = -1;
  if (event.type == YAML_SCALAR_EVENT) {
    *str = strdup((char *)event.data.scalar.value);
    ret = 0;
  }

  yaml_event_delete(&event);
  return ret;
}

static int expected_event_match(const yaml_event_t *event,
                                const expected_event_t *expected_event)
{
  if (event->type != expected_event->event_type) {
    return -1;
  }

  if ((expected_event->event_type == YAML_SCALAR_EVENT) &&
      (expected_event->scalar_value != NULL)) {
    if (strcasecmp((char *)event->data.scalar.value,
                   expected_event->scalar_value) != 0) {
      return -1;
    }
  }

  return 0;
}

static int handle_expected_events(yaml_parser_t *parser,
                                  const expected_event_t *expected_events,
                                  void *context)
{
  yaml_event_t event;
  int event_offset = 0;

  while (expected_events[event_offset].event_type != YAML_NO_EVENT) {

    if (!yaml_parser_parse(parser, &event)) {
      printf("YAML parser error: %d\n", parser->error);
      return -1;
    }

    int i = event_offset;
    while (1) {
      const expected_event_t *expected_event = &expected_events[i];

      if (expected_event_match(&event, expected_event) == 0) {

        debug_printf("process event: %d\n", event.type);
        if (event.type == YAML_SCALAR_EVENT) {
          debug_printf("value: %s\n", event.data.scalar.value);
        }

        /* Process */
        if (expected_event->process != NULL) {
          if (expected_event->process(&event, parser, context) != 0) {
            goto error;
          }
        }

        /* Do not update offset if 'next' is true */
        if (!expected_event->next) {
          event_offset = i + 1;
        }

        break;

      } else if (expected_event->next) {
        i++;
      } else {
        printf("unexpected event: %d\n", event.type);
        goto error;
      }
    }

    yaml_event_delete(&event);
  }

  return 0;

error:
  yaml_event_delete(&event);
  return -1;
}

static port_t * current_port_get(router_t *router)
{
  port_t *port = router->ports_list;
  if (port == NULL) {
    return NULL;
  }

  while (port->next != NULL) {
    port = port->next;
  }

  return port;
}

static forwarding_rule_t * current_forwarding_rule_get(router_t *router)
{
  port_t *port = current_port_get(router);
  if (port == NULL) {
    return NULL;
  }

  forwarding_rule_t *forwarding_rule = port->forwarding_rules_list;
  if (forwarding_rule == NULL) {
    return NULL;
  }

  while (forwarding_rule->next != NULL) {
    forwarding_rule = forwarding_rule->next;
  }

  return forwarding_rule;
}

static filter_t * current_filter_get(router_t *router)
{
  forwarding_rule_t *forwarding_rule = current_forwarding_rule_get(router);
  if (forwarding_rule == NULL) {
    return NULL;
  }

  filter_t *filter = forwarding_rule->filters_list;
  if (filter == NULL) {
    return NULL;
  }

  while (filter->next != NULL) {
    filter = filter->next;
  }

  return filter;
}

static int event_port_string(yaml_parser_t *parser, void *context,
                             size_t offset)
{
  router_t *router = (router_t *)context;

  char *str;
  if (event_scalar_value_get(parser, &str) != 0) {
    return -1;
  }

  port_t *port = current_port_get(router);
  if (port == NULL) {
    return -1;
  }

  *(char **)((char *)port + offset) = str;
  return 0;
}


static PROCESS_FN(router)
{
  debug_printf("process_router\n");
  return handle_expected_events(parser, router_events, context);
}

static PROCESS_FN(router_name)
{
  debug_printf("process_router_name\n");
  router_t *router = (router_t *)context;

  char *str;
  if (event_scalar_value_get(parser, &str) != 0) {
    return -1;
  }

  router->name = str;
  return 0;
}

static PROCESS_FN(ports)
{
  debug_printf("process_ports\n");
  return handle_expected_events(parser, ports_events, context);
}

static PROCESS_FN(port)
{
  debug_printf("process_port\n");
  router_t *router = (router_t *)context;

  port_t **p_next = &router->ports_list;
  while (*p_next != NULL) {
    p_next = &(*p_next)->next;
  }

  port_t *port = (port_t *)malloc(sizeof(*port));
  if (port == NULL) {
    return -1;
  }

  *port = (port_t) {
    .name = "",
    .pub_addr = "",
    .sub_addr = "",
    .pub_socket = NULL,
    .sub_socket = NULL,
    .forwarding_rules_list = NULL,
    .next = NULL
  };

  *p_next = port;
  return handle_expected_events(parser, port_events, context);
}

static PROCESS_FN(port_name)
{
  debug_printf("process_port_name\n");
  return event_port_string(parser, context, offsetof(port_t, name));
}

static PROCESS_FN(pub_addr)
{
  debug_printf("process_pub_addr\n");
  return event_port_string(parser, context, offsetof(port_t, pub_addr));
}

static PROCESS_FN(sub_addr)
{
  debug_printf("process_sub_addr\n");
  return event_port_string(parser, context, offsetof(port_t, sub_addr));
}

static PROCESS_FN(forwarding_rules)
{
  debug_printf("process_forwarding_rules\n");
  return handle_expected_events(parser, forwarding_rules_events, context);
}

static PROCESS_FN(forwarding_rule)
{
  debug_printf("process_forwarding_rule\n");
  router_t *router = (router_t *)context;

  port_t *port = current_port_get(router);
  if (port == NULL) {
    return -1;
  }

  forwarding_rule_t **p_next = &port->forwarding_rules_list;
  while (*p_next != NULL) {
    p_next = &(*p_next)->next;
  }

  forwarding_rule_t *forwarding_rule = (forwarding_rule_t *)
                                           malloc(sizeof(*forwarding_rule));
  if (forwarding_rule == NULL) {
    return -1;
  }

  *forwarding_rule = (forwarding_rule_t) {
    .dst_port_name = "",
    .dst_port = NULL,
    .filters_list = NULL,
    .next = NULL
  };

  *p_next = forwarding_rule;
  return handle_expected_events(parser, forwarding_rule_events, context);
}

static PROCESS_FN(dst_port)
{
  debug_printf("process_dst_port\n");
  router_t *router = (router_t *)context;

  forwarding_rule_t *forwarding_rule = current_forwarding_rule_get(router);
  if (forwarding_rule == NULL) {
    return -1;
  }

  char *str;
  if (event_scalar_value_get(parser, &str) != 0) {
    return -1;
  }

  forwarding_rule->dst_port_name = str;
  return 0;
}

static PROCESS_FN(filters)
{
  debug_printf("process_filters\n");
  return handle_expected_events(parser, filters_events, context);
}

static PROCESS_FN(filter)
{
  debug_printf("process_filter\n");
  router_t *router = (router_t *)context;

  forwarding_rule_t *forwarding_rule = current_forwarding_rule_get(router);
  if (forwarding_rule == NULL) {
    return -1;
  }

  filter_t **p_next = &forwarding_rule->filters_list;
  while (*p_next != NULL) {
    p_next = &(*p_next)->next;
  }

  filter_t *filter = (filter_t *)malloc(sizeof(*filter));
  if (filter == NULL) {
    return -1;
  }

  *filter = (filter_t) {
    .action = FILTER_ACTION_REJECT,
    .data = NULL,
    .len = 0,
    .next = NULL
  };

  *p_next = filter;
  return handle_expected_events(parser, filter_events, context);
}

static PROCESS_FN(action)
{
  debug_printf("process_action\n");
  router_t *router = (router_t *)context;

  filter_t *filter = current_filter_get(router);
  if (filter == NULL) {
    return -1;
  }

  char *str;
  if (event_scalar_value_get(parser, &str) != 0) {
    return -1;
  }

  int ret = 0;
  if (strcasecmp(str, "ACCEPT") == 0) {
    filter->action = FILTER_ACTION_ACCEPT;
  } else if (strcasecmp(str, "REJECT") == 0) {
    filter->action = FILTER_ACTION_REJECT;
  } else {
    ret = -1;
  }

  free(str);
  return ret;
}

static PROCESS_FN(prefix)
{
  debug_printf("process_prefix\n");
  return handle_expected_events(parser, prefix_events, context);
}

static PROCESS_FN(prefix_element)
{
  debug_printf("process_prefix_element\n");
  router_t *router = (router_t *)context;

  filter_t *filter = current_filter_get(router);
  if (filter == NULL) {
    return -1;
  }

  uint8_t b = strtoul((char *)event->data.scalar.value, NULL, 16);

  /* Allocate new buffer */
  uint8_t *buffer = (uint8_t *)malloc(filter->len + 1);
  if (buffer == NULL) {
    return -1;
  }

  /* Copy existing data */
  if (filter->data != NULL) {
    memcpy(buffer, filter->data, filter->len);
    free(filter->data);
  }

  filter->data = buffer;
  filter->data[filter->len++] = b;

  return 0;
}

static int dst_ports_set(router_t *router)
{
  /* Iterate over ports */
  port_t *port;
  for (port = router->ports_list; port != NULL; port = port->next) {

    /* Iterate over forwarding rules */
    forwarding_rule_t *forwarding_rule;
    for (forwarding_rule = port->forwarding_rules_list; forwarding_rule != NULL;
         forwarding_rule = forwarding_rule->next) {

      /* Search for matching destination port name */
      bool found = false;
      port_t *dst_port;
      for (dst_port = router->ports_list; dst_port != NULL;
           dst_port = dst_port->next) {

        if (strcasecmp(forwarding_rule->dst_port_name, dst_port->name) == 0) {
          forwarding_rule->dst_port = dst_port;
          found = true;
          break;
        }
      }

      if (!found) {
        printf("invalid dst_port: %s\n", forwarding_rule->dst_port_name);
        return -1;
      }
    }
  }

  return 0;
}

router_t * zmq_router_load(const char *filename)
{
  FILE *f = NULL;
  router_t *router = NULL;

  yaml_parser_t parser;
  if (!yaml_parser_initialize(&parser)) {
    printf("failed to initialize YAML parser\n");
    return NULL;
  }

  f = fopen(filename, "r");
  if (f == NULL) {
    printf("failed to open %s\n", filename);
    goto error;
  }

  router = (router_t *)malloc(sizeof(*router));
  if (router == NULL) {
    printf("error allocating router\n");
    goto error;
  }

  *router = (router_t) {
    .name = "",
    .ports_list = NULL
  };

  yaml_parser_set_input_file(&parser, f);

  if (process_router(NULL, &parser, router) != 0) {
    printf("error loading %s\n", filename);
    goto error;
  }

  if (dst_ports_set(router) != 0) {
    goto error;
  }

  yaml_parser_delete(&parser);
  fclose(f);

  return router;

error:
  yaml_parser_delete(&parser);

  if (f != NULL) {
    fclose(f);
  }

  if (router != NULL) {
    free(router);
  }

  return NULL;
}
