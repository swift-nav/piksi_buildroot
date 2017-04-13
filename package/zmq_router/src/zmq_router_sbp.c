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
  SBP_PORT_SETTINGS_DAEMON,
  SBP_PORT_EXTERNAL,
  SBP_PORT_FILEIO_FIRMWARE,
  SBP_PORT_FILEIO_EXTERNAL,
  SBP_PORT_INTERNAL,
  SBP_PORT_SETTINGS_CLIENT,
  SBP_PORT_SKYLARK,
} sbp_port_id_t;

static port_t ports_sbp[] = {
  [SBP_PORT_FIRMWARE] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43010",
      .sub_addr = "@tcp://127.0.0.1:43011",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_SETTINGS_DAEMON],
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
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_SKYLARK],
          .filters = (const filter_t *[]){
            &FILTER_ACCEPT(), /* Publish:   SBP MSG_POS_LLH */
            NULL
          }
        },
        NULL
      },
    },
    .pub_socket = NULL,
    .sub_socket = NULL,
  },
  [SBP_PORT_SETTINGS_DAEMON] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43020",
      .sub_addr = "@tcp://127.0.0.1:43021",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_FIRMWARE],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(0x55, 0xA0, 0x00), /* Settings Write */
            &FILTER_REJECT(),
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
          .dst_port = &ports_sbp[SBP_PORT_SETTINGS_CLIENT],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(0x55, 0xA0, 0x00), /* Settings Write */
            &FILTER_REJECT(),
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
          .dst_port = &ports_sbp[SBP_PORT_SETTINGS_DAEMON],
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
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_SETTINGS_CLIENT],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(0x55, 0xA0, 0x00), /* Settings Write */
            &FILTER_REJECT(),
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
  [SBP_PORT_SETTINGS_CLIENT] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43070",
      .sub_addr = "@tcp://127.0.0.1:43071",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_EXTERNAL],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(0x55, 0xA5, 0x00), /* Settings read response */
            &FILTER_REJECT(),
            NULL
          }
        },
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_SETTINGS_DAEMON],
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
  [SBP_PORT_SKYLARK] = {
    .config = {
      .pub_addr = "@tcp://127.0.0.1:43080",
      .sub_addr = "@tcp://127.0.0.1:43081",
      .sub_forwarding_rules = (const forwarding_rule_t *[]) {
        &(forwarding_rule_t){
          .dst_port = &ports_sbp[SBP_PORT_FIRMWARE],
          .filters = (const filter_t *[]) {
            &FILTER_ACCEPT(0x55, 0x44, 0x00), /* Subscribe: SBP MSG_BASE_POS_LLH */
            &FILTER_ACCEPT(0x55, 0x48, 0x00), /* Subscribe: SBP MSG_BASE_POS_ECEF */
            &FILTER_ACCEPT(0x55, 0x4A, 0x00), /* Subscribe: SBP MSG_OBS */
            &FILTER_ACCEPT(0x55, 0x86, 0x00), /* Subscribe: SBP MSG_EPHEMERIS_GPS */
            &FILTER_ACCEPT(0x55, 0x85, 0x00), /* Subscribe: SBP MSG_EPHEMERIS_GLO */
            &FILTER_ACCEPT(0x55, 0x84, 0x00), /* Subscribe: SBP MSG_EPHEMERIS_SBAS */
            &FILTER_REJECT(),
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
