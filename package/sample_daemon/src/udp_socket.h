/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _SWIFT_UDP_SOCKET_H
#define _SWIFT_UDP_SOCKET_H

typedef struct {
  int sock;
  struct sockaddr_storage sock_in;
  size_t sock_in_len;
  sbp_state_t sbp_state;
  size_t buffer_length;
  u8 buffer[1024];
} udp_broadcast_context;

void open_udp_broadcast_socket(udp_broadcast_context *udp_context,
                               const char* broadcast_hostname,
                               int broadcast_port);

void close_udp_broadcast_socket(udp_broadcast_context *udp_context);

u32 udp_write_callback(u8* buf, u32 len, void *context);
void udp_flush_buffer(udp_broadcast_context *udp_context);

#endif//_SWIFT_UDP_SOCKET_H
