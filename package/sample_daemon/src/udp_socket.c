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

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <libsbp/sbp.h>

#include <libpiksi/logging.h>

#include "udp_socket.h"

void open_udp_broadcast_socket(udp_broadcast_context *udp_context,
                               const char* broadcast_hostname,
                               int broadcast_port)
{
  // set up the socket
  int sock;
  struct sockaddr_in sock_in;

  memset(&sock_in, 0, sizeof(sock_in));

  // initalize the socket
  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    sbp_log(LOG_ERR, "open_udp_broadcast_socket: socket() failed: %s", strerror(errno));
  }

  sock_in.sin_addr.s_addr = htonl(INADDR_ANY);
  sock_in.sin_port = htons(0);
  sock_in.sin_family = AF_INET;

  // bind socket, set permissions
  int status = bind(sock, (struct sockaddr *) &sock_in, sizeof(sock_in));

  if (status != 0) {
    sbp_log(LOG_ERR,"bind failed = %d\n", status);
    return;
  }

  int option_value = 1;
  status = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &option_value, sizeof(option_value));

  if (status != 0) {
    sbp_log(LOG_ERR, "setsockopt failed = %d\n", status);
    return;
  }

  option_value = 1;
  status = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option_value, sizeof(option_value));

  if (status != 0) {
    sbp_log(LOG_ERR, "setsockopt failed = %d\n", status);
    return;
  }

  struct sockaddr_storage addr;

  struct addrinfo *resolutions = NULL;
  if (getaddrinfo(broadcast_hostname, NULL, NULL, &resolutions) != 0) {
    sbp_log(LOG_ERR, "address resolution failed");
    return;
  }

  if (resolutions == NULL) {
    sbp_log(LOG_ERR, "no addresses returned by name resolution");
    return;
  }

  memcpy(&addr, resolutions->ai_addr, resolutions->ai_addrlen);

  if (resolutions->ai_family == AF_INET) {
    ((struct sockaddr_in*)&addr)->sin_port = htons(broadcast_port);
  } else if (resolutions->ai_family == AF_INET6) {
    sbp_log(LOG_ERR, "IPv6 is not supported");
    return;
  } else {
    sbp_log(LOG_ERR, "unknown address family returned from name resolution");
    return;
  }

  udp_context->sock = sock;
  udp_context->sock_in_len = resolutions->ai_addrlen;

  memcpy(&udp_context->sock_in, &addr, resolutions->ai_addrlen);
  freeaddrinfo(resolutions);

  sbp_state_init(&udp_context->sbp_state);
  sbp_state_set_io_context(&udp_context->sbp_state, udp_context);
}

void close_udp_broadcast_socket(udp_broadcast_context *udp_context)
{
  if (udp_context->sock < 0) {
    return;
  }

  shutdown(udp_context->sock, SHUT_RDWR);
  close(udp_context->sock);

  udp_context->sock = -1;
}

u32 udp_write_callback(u8* buf, u32 len, void *context)
{
  udp_broadcast_context *udp_context = (udp_broadcast_context*) context;

  if (udp_context->buffer_length >= sizeof(udp_context->buffer))
    return 0;

  memcpy(udp_context->buffer + udp_context->buffer_length, buf, len);
  udp_context->buffer_length += len;

  return len;
}

void udp_flush_buffer(udp_broadcast_context *udp_context)
{
  int res = sendto(udp_context->sock,
                   udp_context->buffer,
                   udp_context->buffer_length,
                   /*flags = */0,
                   (const struct sockaddr*)&udp_context->sock_in,
                   udp_context->sock_in_len);

  if (res < 0) {
    sbp_log(LOG_ERR, "udp_flush_buffer: sendto failed: %s", strerror(errno));
  }

  udp_context->buffer_length = 0;
}
