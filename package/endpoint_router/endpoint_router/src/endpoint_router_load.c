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

#include <stddef.h>

#include <yaml.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "endpoint_router_load.h"

#define PROCESS_FN(name) \
  int process_##name(yaml_event_t *event, yaml_parser_t *parser, void *context)

typedef struct { /* NOLINT */
  yaml_event_type_t event_type;
  const char *scalar_value;
  int (*process)(yaml_event_t *event, yaml_parser_t *parser, void *context);
  bool next;
} expected_event_t;

static bool is_valid_metric_name(const char *s);

static PROCESS_FN(router);
static PROCESS_FN(router_name);
static PROCESS_FN(ports);
static PROCESS_FN(port);
static PROCESS_FN(port_name);
static PROCESS_FN(port_metric);
static PROCESS_FN(pub_addr);
static PROCESS_FN(sub_addr);
static PROCESS_FN(forwarding_rules_);
static PROCESS_FN(forwarding_rule_);
static PROCESS_FN(dst_port);
static PROCESS_FN(filters);
static PROCESS_FN(filter);
static PROCESS_FN(action);
static PROCESS_FN(prefix);
static PROCESS_FN(prefix_element);

static expected_event_t router_events[] = {
  {YAML_STREAM_START_EVENT, NULL, NULL, false},
  {YAML_DOCUMENT_START_EVENT, NULL, NULL, false},
  {YAML_MAPPING_START_EVENT, NULL, NULL, false},
  {YAML_SCALAR_EVENT, "name", process_router_name, true},
  {YAML_SCALAR_EVENT, "ports", process_ports, true},
  {YAML_MAPPING_END_EVENT, NULL, NULL, false},
  {YAML_DOCUMENT_END_EVENT, NULL, NULL, false},
  {YAML_STREAM_END_EVENT, NULL, NULL, false},
  {YAML_NO_EVENT, NULL, NULL, false},
};

static expected_event_t ports_events[] = {
  {YAML_SEQUENCE_START_EVENT, NULL, NULL, false},
  {YAML_MAPPING_START_EVENT, NULL, process_port, true},
  {YAML_SEQUENCE_END_EVENT, NULL, NULL, false},
  {YAML_NO_EVENT, NULL, NULL, false},
};

static expected_event_t port_events[] = {
  {YAML_SCALAR_EVENT, "name", process_port_name, true},
  {YAML_SCALAR_EVENT, "metric", process_port_metric, true},
  {YAML_SCALAR_EVENT, "pub_addr", process_pub_addr, true},
  {YAML_SCALAR_EVENT, "sub_addr", process_sub_addr, true},
  {YAML_SCALAR_EVENT, "forwarding_rules", process_forwarding_rules_, true},
  {YAML_MAPPING_END_EVENT, NULL, NULL, false},
  {YAML_NO_EVENT, NULL, NULL, false},
};

static expected_event_t forwarding_rules_events[] = {
  {YAML_SEQUENCE_START_EVENT, NULL, NULL, false},
  {YAML_MAPPING_START_EVENT, NULL, process_forwarding_rule_, true},
  {YAML_SEQUENCE_END_EVENT, NULL, NULL, false},
  {YAML_NO_EVENT, NULL, NULL, false},
};

static expected_event_t forwarding_rule_events[] = {
  {YAML_SCALAR_EVENT, "dst_port", process_dst_port, true},
  {YAML_SCALAR_EVENT, "filters", process_filters, true},
  {YAML_MAPPING_END_EVENT, NULL, NULL, false},
  {YAML_NO_EVENT, NULL, NULL, false},
};

static expected_event_t filters_events[] = {
  {YAML_SEQUENCE_START_EVENT, NULL, NULL, false},
  {YAML_MAPPING_START_EVENT, NULL, process_filter, true},
  {YAML_SEQUENCE_END_EVENT, NULL, NULL, false},
  {YAML_NO_EVENT, NULL, NULL, false},
};

static expected_event_t filter_events[] = {
  {YAML_SCALAR_EVENT, "action", process_action, true},
  {YAML_SCALAR_EVENT, "prefix", process_prefix, true},
  {YAML_MAPPING_END_EVENT, NULL, NULL, false},
  {YAML_NO_EVENT, NULL, NULL, false},
};

static expected_event_t prefix_events[] = {
  {YAML_SEQUENCE_START_EVENT, NULL, NULL, false},
  {YAML_SCALAR_EVENT, NULL, process_prefix_element, true},
  {YAML_SEQUENCE_END_EVENT, NULL, NULL, false},
  {YAML_NO_EVENT, NULL, NULL, false},
};

static int event_scalar_value_get(yaml_parser_t *parser, char **str)
{
  yaml_event_t event;
  if (!yaml_parser_parse(parser, &event)) {
    router_log(LOG_ERR, "YAML parser error: %d\n", parser->error);
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

static int expected_event_match(const yaml_event_t *event, const expected_event_t *expected_event)
{
  if (event->type != expected_event->event_type) {
    return -1;
  }

  if ((expected_event->event_type == YAML_SCALAR_EVENT) && (expected_event->scalar_value != NULL)) {
    if (strcasecmp((char *)event->data.scalar.value, expected_event->scalar_value) != 0) {
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
      router_log(LOG_ERR, "YAML parser error: %d\n", parser->error);
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
      }

      if (expected_event->next) {
        i++;
      } else {
        router_log(LOG_ERR, "unexpected event: %d\n", event.type);
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

static port_t *current_port_get(router_t *router)
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

static forwarding_rule_t *current_forwarding_rule_get(router_t *router)
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

static filter_t *current_filter_get(router_t *router)
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

NESTED_FN_TYPEDEF(int, consume_str_fn_t, port_t *p, char *);

#define MAKE_CONSUME_FN(FuncName, TheField)                           \
  consume_str_fn_t FuncName = NESTED_FN(int, (port_t * p, char *s), { \
    p->TheField = s;                                                  \
    return 0;                                                         \
  });

static int event_port_string(yaml_parser_t *parser, void *context, consume_str_fn_t consume_str_fn)
{
  router_t *router = (router_t *)context;
  port_t *port = NULL;

  char *str = NULL;
  if (event_scalar_value_get(parser, &str) != 0) goto error;

  port = current_port_get(router);
  if (port == NULL) goto error;

  return consume_str_fn(port, str);

error:
  if (str != NULL) free(str);
  return -1;
}


static PROCESS_FN(router)
{
  (void)event;

  debug_printf("process_router\n");
  return handle_expected_events(parser, router_events, context);
}

static PROCESS_FN(router_name)
{
  (void)event;

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
  (void)event;

  debug_printf("process_ports\n");
  return handle_expected_events(parser, ports_events, context);
}

static PROCESS_FN(port)
{
  (void)event;

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

  *port = (port_t){
    .name = "",
    .metric = "",
    .pub_addr = "",
    .sub_addr = "",
    .pub_ept = NULL,
    .sub_ept = NULL,
    .forwarding_rules_list = NULL,
    .next = NULL,
  };

  *p_next = port;

  int rc = handle_expected_events(parser, port_events, context);
  if (rc != 0) return rc;

  if (!is_valid_metric_name(port->metric)) {
    return -1;
  }

  return 0;
}

static PROCESS_FN(port_name)
{
  (void)event;
  debug_printf("%s\n", __FUNCTION__);
  MAKE_CONSUME_FN(assign_name, name);
  return event_port_string(parser, context, assign_name);
}

static bool is_valid_metric_name(const char *s)
{
  if (s == NULL) return false;

  size_t s_size = strlen(s);

  for (size_t x = 0; x < s_size; x++) {
    if (!isspace(s[x])) return true;
  }

  router_log(LOG_ERR, "metric name must be non-empty and not entirely whitespace\n");
  return false;
}

static PROCESS_FN(port_metric)
{
  (void)event;
  debug_printf("%s\n", __FUNCTION__);
  consume_str_fn_t assign_metric = NESTED_FN(int, (port_t * p, char *s), {
    if (!is_valid_metric_name(s)) return -1;
    p->metric = s;
    return 0;
  });
  return event_port_string(parser, context, assign_metric);
}

static PROCESS_FN(pub_addr)
{
  (void)event;
  debug_printf("%s\n", __FUNCTION__);
  MAKE_CONSUME_FN(assign_pub_addr, pub_addr);
  return event_port_string(parser, context, assign_pub_addr);
}

static PROCESS_FN(sub_addr)
{
  (void)event;
  debug_printf("%s\n", __FUNCTION__);
  MAKE_CONSUME_FN(assign_sub_addr, sub_addr);
  return event_port_string(parser, context, assign_sub_addr);
}

static PROCESS_FN(forwarding_rules_)
{
  (void)event;
  debug_printf("%s\n", __FUNCTION__);
  return handle_expected_events(parser, forwarding_rules_events, context);
}

static PROCESS_FN(forwarding_rule_)
{
  (void)event;

  debug_printf("%s\n", __FUNCTION__);
  router_t *router = (router_t *)context;

  port_t *port = current_port_get(router);
  if (port == NULL) {
    return -1;
  }

  forwarding_rule_t **p_next = &port->forwarding_rules_list;
  while (*p_next != NULL) {
    p_next = &(*p_next)->next;
  }

  forwarding_rule_t *forwarding_rule = (forwarding_rule_t *)malloc(sizeof(*forwarding_rule));
  if (forwarding_rule == NULL) {
    return -1;
  }

  *forwarding_rule = (forwarding_rule_t){
    .dst_port_name = "",
    .dst_port = NULL,
    .filters_list = NULL,
    .next = NULL,
  };

  *p_next = forwarding_rule;
  return handle_expected_events(parser, forwarding_rule_events, context);
}

static PROCESS_FN(dst_port)
{
  (void)event;

  debug_printf("%s\n", __FUNCTION__);
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
  (void)event;

  debug_printf("%s\n", __FUNCTION__);
  return handle_expected_events(parser, filters_events, context);
}

static PROCESS_FN(filter)
{
  (void)event;

  debug_printf("%s\n", __FUNCTION__);
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

  *filter = (filter_t){
    .action = FILTER_ACTION_REJECT,
    .data = NULL,
    .len = 0,
    .next = NULL,
  };

  *p_next = filter;
  return handle_expected_events(parser, filter_events, context);
}

static PROCESS_FN(action)
{
  (void)event;

  debug_printf("%s\n", __FUNCTION__);
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
  (void)event;

  debug_printf("%s\n", __FUNCTION__);
  return handle_expected_events(parser, prefix_events, context);
}

static PROCESS_FN(prefix_element)
{
  (void)event;
  (void)parser;

  debug_printf("%s\n", __FUNCTION__);
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
      for (dst_port = router->ports_list; dst_port != NULL; dst_port = dst_port->next) {

        if (strcasecmp(forwarding_rule->dst_port_name, dst_port->name) == 0) {
          forwarding_rule->dst_port = dst_port;
          found = true;
          break;
        }
      }

      if (!found) {
        router_log(LOG_ERR, "invalid dst_port: %s\n", forwarding_rule->dst_port_name);
        return -1;
      }
    }
  }

  return 0;
}

router_t *router_load(const char *filename)
{
  FILE *f = NULL;
  router_t *router = NULL;

  yaml_parser_t parser;
  if (!yaml_parser_initialize(&parser)) {
    router_log(LOG_ERR, "failed to initialize YAML parser\n");
    return NULL;
  }

  f = fopen(filename, "r");
  if (f == NULL) {
    router_log(LOG_ERR, "failed to open %s\n", filename);
    goto error;
  }

  router = (router_t *)malloc(sizeof(*router));
  if (router == NULL) {
    router_log(LOG_ERR, "error allocating router\n");
    goto error;
  }

  *router = (router_t){
    .name = "",
    .ports_list = NULL,
  };

  yaml_parser_set_input_file(&parser, f);

  if (process_router(NULL, &parser, router) != 0) {
    router_log(LOG_ERR, "error loading %s\n", filename);
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

static void filters_destroy(filter_t **filter_loc)
{
  if (filter_loc == NULL || *filter_loc == NULL) {
    return;
  }
  filter_t *filter = *filter_loc;
  filter_t *next = NULL;
  while (filter != NULL) {
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
  while (forwarding_rule != NULL) {
    next = forwarding_rule->next;
    if (forwarding_rule->dst_port_name != NULL && forwarding_rule->dst_port_name[0] != '\0') {
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
  while (port != NULL) {
    next = port->next;
    if (port->name != NULL && port->name[0] != '\0') {
      free((void *)port->name);
    }
    if (port->metric != NULL && port->metric[0] != '\0') {
      free((void *)port->metric);
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

void router_teardown(router_t **router_loc)
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
