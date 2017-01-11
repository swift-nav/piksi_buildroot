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

#ifndef SWIFTNAV_SBP_ZMQ_H
#define SWIFTNAV_SBP_ZMQ_H

#include <stdint.h>
#include <libsbp/sbp.h>
#include <czmq.h>

typedef struct {
  u16 sbp_sender_id;
  const char *pub_endpoint;
  const char *sub_endpoint;
} sbp_zmq_config_t;

typedef struct {
  sbp_state_t *sbp_state;
  u16 sbp_sender_id;
  zsock_t *pub;
  zsock_t *sub;
  zloop_t *zloop;
  bool interrupt;
} sbp_zmq_state_t;

int sbp_zmq_init(sbp_zmq_state_t *s, const sbp_zmq_config_t *config);
int sbp_zmq_deinit(sbp_zmq_state_t *s);
int sbp_zmq_callback_register(sbp_zmq_state_t *s, u16 msg_type,
                              sbp_msg_callback_t cb, void *context,
                              sbp_msg_callbacks_node_t **node);
int sbp_zmq_callback_remove(sbp_zmq_state_t *s,
                            sbp_msg_callbacks_node_t *node);
int sbp_zmq_message_send(sbp_zmq_state_t *s, u16 msg_type, u8 len, u8 *payload);
int sbp_zmq_loop(sbp_zmq_state_t *s);
int sbp_zmq_loop_timeout(sbp_zmq_state_t *s, u32 timeout_ms);
int sbp_zmq_loop_interrupt(sbp_zmq_state_t *s);
zloop_t *sbp_zmq_loop_get(sbp_zmq_state_t *s);

#endif /* SWIFTNAV_SBP_ZMQ_H */
