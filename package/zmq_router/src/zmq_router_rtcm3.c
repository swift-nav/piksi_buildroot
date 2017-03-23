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

typedef enum {
  RTCM3_PORT_INTERNAL,
  RTCM3_PORT_EXTERNAL
} rtcm3_port_id_t;

static port_t ports_rtcm3[] = {
  [RTCM3_PORT_INTERNAL] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:45010",
      .sub_addr = "@tcp://127.0.0.1:45011",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        NULL
      },
    },
    .pub_socket = NULL,
    .sub_socket = NULL,
  },
  [RTCM3_PORT_EXTERNAL] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:45030",
      .sub_addr = "@tcp://127.0.0.1:45031",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_rtcm3[RTCM3_PORT_INTERNAL],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(),
            NULL
          }
        },
        NULL
      },
    },
    .pub_socket = NULL,
    .sub_socket = NULL,
  }
};

const router_t router_rtcm3 = {
  .ports = ports_rtcm3,
  .ports_count = sizeof(ports_rtcm3)/sizeof(ports_rtcm3[0]),
};
