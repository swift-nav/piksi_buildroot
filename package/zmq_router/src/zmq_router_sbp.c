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
  SBP_PORT_FIRMWARE,
  SBP_PORT_SETTINGS,
  SBP_PORT_EXTERNAL
} sbp_port_id_t;

static port_t ports_sbp[] = {
  [SBP_PORT_FIRMWARE] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43010",
      .sub_addr = "@tcp://127.0.0.1:43011",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_SETTINGS],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_EXTERNAL],
          .filters = (const filter_t *[]){
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
  [SBP_PORT_SETTINGS] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43020",
      .sub_addr = "@tcp://127.0.0.1:43021",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_FIRMWARE],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_EXTERNAL],
          .filters = (const filter_t *[]){
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
  [SBP_PORT_EXTERNAL] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43030",
      .sub_addr = "@tcp://127.0.0.1:43031",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_FIRMWARE],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_SETTINGS],
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

const router_t router_sbp = {
  .ports = ports_sbp,
  .ports_count = sizeof(ports_sbp)/sizeof(ports_sbp[0]),
};
