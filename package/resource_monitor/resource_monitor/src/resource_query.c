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
  if (resq->init != NULL) {
    node->context = resq->init();
    if (node->context == NULL) {
      piksi_log(LOG_ERR, "query module '%s' failed to initialize", resq->describe());
      free(node);
      return;
    }
  }
  LIST_INSERT_HEAD(&interface_list, node, entries);
}

void resq_run_all(void)
{
  u16 msg_type;
  u8 msg_len;
  u8 buf[SBP_PAYLOAD_SIZE_MAX];
  resq_node_t *node;
  LIST_FOREACH(node, &interface_list, entries)
  {
    node->query->run_query(node->context);
    while (node->query->prepare_sbp(&msg_type, &msg_len, buf, node->context)) {
      fprintf(stderr, "%s: sending sbp, type=%d, len=%d\n", __FUNCTION__, msg_type, msg_len);
      sbp_tx_send(sbp_get_tx_ctx(), msg_type, msg_len, buf);
    }
  }
}

void resq_destroy_all(void)
{
  resq_node_t *node = NULL;
  while (!LIST_EMPTY(&interface_list)) {
    node = LIST_FIRST(&interface_list);
    LIST_REMOVE(node, entries);
    if (node->query->teardown != NULL) {
      node->query->teardown(&node->context);
    }
    free(node);
  }
}
