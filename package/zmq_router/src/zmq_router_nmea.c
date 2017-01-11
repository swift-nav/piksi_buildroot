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
  NMEA_PORT_FIRMWARE,
  NMEA_PORT_EXTERNAL
} nmea_port_id_t;

static port_t ports_nmea[] = {
  [NMEA_PORT_FIRMWARE] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:44010",
      .sub_addr = "@tcp://127.0.0.1:44011",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_nmea[NMEA_PORT_EXTERNAL],
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
  },
  [NMEA_PORT_EXTERNAL] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:44030",
      .sub_addr = "@tcp://127.0.0.1:44031",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        NULL
      },
    },
    .pub_socket = NULL,
    .sub_socket = NULL,
  }
};

const router_t router_nmea = {
  .ports = ports_nmea,
  .ports_count = sizeof(ports_nmea)/sizeof(ports_nmea[0]),
};
