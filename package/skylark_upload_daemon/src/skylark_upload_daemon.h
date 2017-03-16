/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Mark Fine <mark@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_SKYLARK_UPLOAD_DAEMON_H
#define SWIFTNAV_SKYLARK_UPLOAD_DAEMON_H

//
// Upload Daemon - connects to Skylark and sends SBP messages.
//

#include <stdlib.h>
#include <unistd.h>

#include <sbp_zmq.h>

// Callback used by libcurl to pass data to write to Skylark. Reads from pipe.
//
size_t upload_callback(void *p, size_t size, size_t n, void *up);

// Upload process. Calls libcurl to connect to Skylark. Takes a pipe fd.
//
void upload(int fd);

// Test source process. Takes a pipe fd and writes to it from STDIN.
//
void source(int fd);

// Msg writing callback for msg callback. Takes a pipe fd and writes a message
// to it.
//
u32 msg_write(u8 *buf, u32 n, void *context);

// SBP msg callback - writes message to pipe.
//
void msg_callback(u16 sender_id, u8 len, u8 msg[], void *context);

// zmq read loop process. Takes a pipe fd and writes messages to it from
// listening to SBP zmq.
//
void msg_loop(int fd);

#endif /* SWIFTNAV_SKYLARK_UPLOAD_DAEMON_H */
