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
  SBP_PORT_EXTERNAL,
  SBP_PORT_FILEIO_FIRMWARE,
  SBP_PORT_FILEIO_EXTERNAL,
  SBP_PORT_INTERNAL,
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
            &FILTER_ACCEPT(0x55, 0xAE, 0x00), /* Settings register */
            &FILTER_ACCEPT(0x55, 0xA5, 0x00), /* Settings read response */
            &FILTER_REJECT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_EXTERNAL],
          .filters = (const filter_t *[]){
            &FILTER_REJECT(0x55, 0xAE, 0x00), /* Settings register */
            &FILTER_REJECT(0x55, 0xA8, 0x00), /* File read */
            &FILTER_REJECT(0x55, 0xA9, 0x00), /* File read dir */
            &FILTER_REJECT(0x55, 0xAC, 0x00), /* File remove */
            &FILTER_REJECT(0x55, 0xAD, 0x00), /* File write */
            &FILTER_ACCEPT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_FILEIO_FIRMWARE],
          .filters = (const filter_t *[]){
            &FILTER_ACCEPT(0x55, 0xA8, 0x00), /* File read */
            &FILTER_ACCEPT(0x55, 0xA9, 0x00), /* File read dir */
            &FILTER_ACCEPT(0x55, 0xAC, 0x00), /* File remove */
            &FILTER_ACCEPT(0x55, 0xAD, 0x00), /* File write */
            &FILTER_REJECT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_INTERNAL],
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
            &FILTER_REJECT(0x55, 0xA0, 0x00), /* Settings Write */
            &FILTER_ACCEPT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_INTERNAL],
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
            &FILTER_REJECT(0x55, 0xAE, 0x00), /* Settings register */
            &FILTER_REJECT(0x55, 0xA5, 0x00), /* Settings read response */
            &FILTER_ACCEPT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_SETTINGS],
          .filters = (const filter_t *[]) {
            &FILTER_REJECT(0x55, 0xAE, 0x00), /* Settings register */
            &FILTER_REJECT(0x55, 0xA5, 0x00), /* Settings read response */
            &FILTER_ACCEPT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_FILEIO_EXTERNAL],
          .filters = (const filter_t *[]){
            &FILTER_ACCEPT(0x55, 0xA8, 0x00), /* File read */
            &FILTER_ACCEPT(0x55, 0xA9, 0x00), /* File read dir */
            &FILTER_ACCEPT(0x55, 0xAC, 0x00), /* File remove */
            &FILTER_ACCEPT(0x55, 0xAD, 0x00), /* File write */
            &FILTER_REJECT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_INTERNAL],
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
  [SBP_PORT_FILEIO_FIRMWARE] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43040",
      .sub_addr = "@tcp://127.0.0.1:43041",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_FIRMWARE],
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
  [SBP_PORT_FILEIO_EXTERNAL] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43050",
      .sub_addr = "@tcp://127.0.0.1:43051",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_EXTERNAL],
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
  [SBP_PORT_INTERNAL] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43060",
      .sub_addr = "@tcp://127.0.0.1:43061",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_SETTINGS],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(0x55, 0xAE, 0x00), /* Settings register */
            &FILTER_ACCEPT(0x55, 0xA5, 0x00), /* Settings read response */
            &FILTER_REJECT(),
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
  }
};

const router_t router_sbp = {
  .ports = ports_sbp,
  .ports_count = sizeof(ports_sbp)/sizeof(ports_sbp[0]),
};
