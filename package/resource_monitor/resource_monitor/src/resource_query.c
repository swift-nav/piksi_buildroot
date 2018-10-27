/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "resource_query.h"

#include <sys/queue.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_pubsub.h>

#include "sbp.h"


typedef struct resq_node {
  bool init_success;
  void *context;
  resq_interface_t *query;
  LIST_ENTRY(resq_node) entries;
} resq_node_t;

typedef LIST_HEAD(resq_nodes_head, resq_node) resq_node_head_t;

resq_node_head_t interface_list = LIST_HEAD_INITIALIZER(resq_nodes_head);

void resq_register(resq_interface_t *resq)
{
  resq_node_t *node = calloc(1, sizeof(resq_node_t));
  node->query = resq;

  LIST_INSERT_HEAD(&interface_list, node, entries);
}

void resq_initialize_all(void)
{
  resq_node_t *node;
  for (resq_priority_t priority = RESQ_PRIORITY_1; priority < RESQ_PRIORITY_COUNT; priority++) {
    LIST_FOREACH(node, &interface_list, entries)
    {
      if (node->query->priority == priority) {
        node->init_success = true;
        if (node->query->init != NULL) {
          node->context = node->query->init();
          if (node->context == NULL) {
            piksi_log(LOG_ERR, "query module '%s' failed to initialize", node->query->describe());
            node->init_success = false;
          }
        }
      }
    }
  }
}

void resq_run_all(void)
{
  u16 msg_type;
  u8 msg_len;
  u8 buf[SBP_PAYLOAD_SIZE_MAX];
  resq_node_t *node;
  LIST_FOREACH(node, &interface_list, entries)
  {
    if (!node->init_success) continue;
    node->query->run_query(node->context);
    while (node->query->prepare_sbp(&msg_type, &msg_len, buf, node->context)) {
      fprintf(stderr, "%s: sending sbp, type=%d, len=%d\n", __FUNCTION__, msg_type, msg_len);
      sbp_tx_send(sbp_get_tx_ctx(), msg_type, msg_len, buf);
    }
  }
}

bool resq_read_property(const char *query_name, resq_read_property_t *read_prop)
{
  if (read_prop->type == RESQ_PROP_NONE) {
    PK_LOG_ANNO(LOG_ERR, "invalid property type requested: %d", read_prop->type);
    return false;
  }

  resq_node_t *node = NULL;
  LIST_FOREACH(node, &interface_list, entries)
  {
    if (!node->init_success) continue;

    if (node->query->read_property == NULL) continue;

    const char *name = node->query->describe();
    if (strcmp(query_name, name) != 0) continue;

    resq_read_property_t read_prop1 = {.id = read_prop->id, .type = RESQ_PROP_NONE};

    if (!node->query->read_property(&read_prop1, node->context)) {
      PK_LOG_ANNO(LOG_ERR,
                  "invalid property requested: %d, from module: %s",
                  read_prop->id,
                  query_name);
      return false;
    }

    if (read_prop1.type == RESQ_PROP_NONE) {
      PK_LOG_ANNO(LOG_ERR,
                  "type was not set on outgoing property: %d, from module: %s",
                  read_prop->id,
                  query_name);
      return false;
    }

    if (read_prop1.type != read_prop->type) {
      PK_LOG_ANNO(LOG_ERR,
                  "invalid type for property: %d, expected: %d, from module: %s",
                  read_prop->id,
                  read_prop1.type,
                  query_name);
      return false;
    }

    *read_prop = read_prop1;
    return true;
  }

  PK_LOG_ANNO(LOG_ERR,
              "could not find module with name '%s' that could read property: %d",
              query_name,
              read_prop->id);
  return false;
}

void resq_destroy_all(void)
{
  resq_node_t *node = NULL;
  while (!LIST_EMPTY(&interface_list)) {
    node = LIST_FIRST(&interface_list);
    LIST_REMOVE(node, entries);
    if (node->init_success && node->query->teardown != NULL) {
      node->query->teardown(&node->context);
    }
    free(node);
  }
}
