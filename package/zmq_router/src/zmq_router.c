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

#include <assert.h>

#include "zmq_router.h"

extern const router_t router_sbp;
extern const router_t router_nmea;

static const router_t * const routers[] = {
  &router_sbp,
  &router_nmea
};

static void router_setup(const router_t *router)
{
  for (int i=0; i<router->ports_count; i++) {
    port_t *port = &router->ports[i];

    port->pub_socket = zsock_new_pub(port->config.pub_addr);
    if (port->pub_socket == NULL) {
      printf("zsock_new_pub() error\n");
      exit(1);
    }

    port->sub_socket = zsock_new_sub(port->config.sub_addr, "");
    if (port->sub_socket == NULL) {
      printf("zsock_new_sub() error\n");
      exit(1);
    }
  }
}

static void routers_setup(const router_t * const routers[], int routers_count)
{
  for (int i=0; i<routers_count; i++) {
    router_setup(routers[i]);
  }
}

static void router_destroy(const router_t *router)
{
  for (int i=0; i<router->ports_count; i++) {
    port_t *port = &router->ports[i];
    zsock_destroy(&port->pub_socket);
    assert(port->pub_socket == NULL);
    zsock_destroy(&port->sub_socket);
    assert(port->sub_socket == NULL);
  }
}

static void routers_destroy(const router_t * const routers[], int routers_count)
{
  for (int i=0; i<routers_count; i++) {
    router_destroy(routers[i]);
  }
}

static void loop_add_router(zloop_t *loop, const router_t *router,
                            zloop_reader_fn reader_fn)
{
  for (int i=0; i<router->ports_count; i++) {
    port_t *port = &router->ports[i];
    int result;
    result = zloop_reader(loop, port->sub_socket, reader_fn, port);
    if (result != 0) {
      printf("zloop_reader() error\n");
      exit(1);
    }
  }
}

static void loop_setup(zloop_t *loop, const router_t * const routers[],
                       int routers_count, zloop_reader_fn reader_fn)
{
  for (int i=0; i<routers_count; i++) {
    loop_add_router(loop, routers[i], reader_fn);
  }
}

static void process_filter_match(const forwarding_rule_t *forwarding_rule,
                                 const filter_t *filter, zmsg_t *msg)
{
  switch (filter->action) {
  case FILTER_ACTION_ACCEPT: {
    zmsg_t *tx_msg = zmsg_dup(msg);
    if (tx_msg == NULL) {
      printf("zmsg_dup() error\n");
      break;
    }
    int result = zmsg_send(&tx_msg, forwarding_rule->dst_port->pub_socket);
    if (result != 0) {
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

static void process_rule(const forwarding_rule_t *forwarding_rule,
                         const void *prefix, int prefix_len, zmsg_t *msg)
{
  /* Iterate over filters for this rule */
  int filter_index = 0;
  while (1) {
    const filter_t *filter = forwarding_rule->filters[filter_index++];
    if (filter == NULL) {
      break;
    }

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
      process_filter_match(forwarding_rule, filter, msg);

      /* Done with this rule after finding a filter match */
      break;
    }
  }
}

static int reader_fn(zloop_t *loop, zsock_t *reader, void *arg)
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
  int rx_prefix_len = 0;
  if (rx_frame_first != NULL) {
    rx_prefix = zframe_data(rx_frame_first);
    rx_prefix_len = zframe_size(rx_frame_first);
  }

  /* Iterate over forwarding rules */
  int rule_index = 0;
  while (1) {
    const forwarding_rule_t *forwarding_rule =
        port->config.sub_forwarding_rules[rule_index++];
    if (forwarding_rule == NULL) {
      break;
    }

    process_rule(forwarding_rule, rx_prefix, rx_prefix_len, rx_msg);
  }

  zmsg_destroy(&rx_msg);
  return 0;
}

int main (void)
{
  routers_setup(routers, sizeof(routers)/sizeof(routers[0]));

  zloop_t *loop = zloop_new();
  assert(loop);
  loop_setup(loop, routers, sizeof(routers)/sizeof(routers[0]), reader_fn);

  zloop_start(loop);

  zloop_destroy(&loop);
  routers_destroy(routers, sizeof(routers)/sizeof(routers[0]));

  return 0;
}
