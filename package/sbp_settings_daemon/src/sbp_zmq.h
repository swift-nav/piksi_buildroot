/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef __SBP_ZMQ_H
#define __SBP_ZMQ_H

#include <libsbp/sbp.h>
#include <czmq.h>

/** Value defining maximum SBP packet size */
#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

sbp_state_t *sbp_zmq_init(void);
void sbp_zmq_send_msg(sbp_state_t *s, u16 msg_type, u8 len, u8 buff[]);
s8 sbp_zmq_register_callback(sbp_state_t *s, u16 msg_type, sbp_msg_callback_t cb);
void sbp_zmq_loop(sbp_state_t *s);

#endif

