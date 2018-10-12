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

typedef struct resq_node {
  void* context;
  resq_interface_t *query;
  LIST_ENTRY(resq_node) entries;
} resq_node_t;

typedef LIST_HEAD(resq_nodes_head, resq_node) resq_node_head_t;

resq_node_head_t interface_list = LIST_HEAD_INITIALIZER(resq_nodes_head);

void resq_register(resq_interface_t *resq)
{
  resq_node_t *node = malloc(sizeof(resq_node_t));

  node->query = resq;
  node->context = resq->init();

  LIST_INSERT_HEAD(&interface_list, node, entries);
}

void resq_run_all(void) 
{
  resq_node_t *node;

  LIST_FOREACH(node, &interface_list, entries)
  {
    node->query->run_query(node->context);
  }
}

void resq_destroy_all(void)
{
  resq_node_t *node = NULL;

  while (!LIST_EMPTY(&interface_list)) {
    node = LIST_FIRST(&interface_list);
    LIST_REMOVE(node, entries);
    free(node);
  }
}
