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

#ifndef SWIFTNAV_SKYLARK_DOWNLOAD_DAEMON_H
#define SWIFTNAV_SKYLARK_DOWNLOAD_DAEMON_H

//
// Download Daemon - connects to Skylark and receives SBP messages.
//

#include <stdlib.h>
#include <unistd.h>

#include <sbp_zmq.h>

// Callback used by libcurl to pass data read from Skylark. Writes to pipe.
//
size_t download_callback(void *p, size_t size, size_t n, void *up);

// Download process. Calls libcurl to connect to Skylark. Takes a pipe fd.
//
void download(int fd);

// Test sink process. Takes a pipe fd and reads from it to STDOUT.
//
void sink(int fd);

// Msg reading callback for sbp_process. Reads from pipe.
//
u32 msg_read(u8 *buf, u32 n, void *context);

// SBP msg callback - sends message to SBP zmq.
//
void msg_callback(u16 sender_id, u8 len, u8 msg[], void *context);

void base_msg_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context);

void base_msg_ecef_callback(u16 sender_id, u8 len, u8 msg[], void *context);

// Read loop process. Takes a pipe fd and reads from it to send to SBP zmq.
//
void msg_loop(int fd);

#endif /* SWIFTNAV_SKYLARK_DOWNLOAD_DAEMON_H */
