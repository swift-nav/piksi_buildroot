/*
 * Copyright (C) 2018-2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#define _GNU_SOURCE

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <linux/can/raw.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "endpoint_adapter.h"

#define CAN_SLEEP_NS 1000000 /* 1 millisecond */

#define READ 0
#define WRITE 1

#define BUFFER_SIZE (4096)
static uint8_t pipe_read_buffer[BUFFER_SIZE];

static u32 can_filter;

typedef struct {
  int can_read_fd;
  int read_pipe_fd;
} read_thread_context_t;

typedef struct {
  int can_write_fd;
  int write_pipe_fd;
} write_thread_context_t;

static void *can_read_thread_handler(void *arg)
{
  piksi_log(LOG_DEBUG, "CAN read thread starting...");
  read_thread_context_t *pctx = (read_thread_context_t *)arg;

  for (;;) {

    fd_set fds;
    FD_ZERO(&fds);

    FD_SET(pctx->can_read_fd, &fds);

    /* "Non-blocking", select with a zero time-out value causes it to return immediately */
    struct timeval tv = {0, 0};

    if (select((pctx->can_read_fd + 1), &fds, NULL, NULL, &tv) < 0) {
      debug_printf("select failed: %s (%d)", strerror(errno), errno);
      return 0;
    }

    if (FD_ISSET(pctx->can_read_fd, &fds) == 0) {
      nanosleep((const struct timespec[]){{0, CAN_SLEEP_NS}}, NULL);
      continue;
    }

    struct can_frame frame = {0};
    ssize_t read_result = read(pctx->can_read_fd, &frame, sizeof(frame));

    /* Retry if interrupted */
    if ((read_result == -1) && (errno == EINTR)) {
      continue;

    } else {

      if (frame.can_dlc == 0) {

        PK_LOG_ANNO(LOG_WARNING, "CAN read failed: %s (%d)", strerror(errno), errno);
        nanosleep((const struct timespec[]){{0, CAN_SLEEP_NS}}, NULL);

        continue;
      }

      if (write(pctx->read_pipe_fd, frame.data, frame.can_dlc) < 0) {

        PK_LOG_ANNO(LOG_WARNING, "pipe write() failed: %s (%d)", strerror(errno), errno);
        break;
      }
    }
  }

  piksi_log(LOG_DEBUG, "CAN read thread stopping...");
  return NULL;
}

static ssize_t can_write(int fd, const void *buffer, size_t count)
{
  struct can_frame frame = {0};

  frame.can_id = can_filter & CAN_SFF_MASK;
  frame.can_dlc = sizeof(frame.data);

  if (count < frame.can_dlc) {
    frame.can_dlc = count;
  }

  memcpy(frame.data, buffer, frame.can_dlc);

  for (;;) {

    ssize_t ret = write(fd, &frame, sizeof(frame));

    if ((ret == -1) && (errno == EINTR)) {

      /* Retry if interrupted */

      nanosleep((const struct timespec[]){{0, CAN_SLEEP_NS}}, NULL);
      piksi_log(LOG_WARNING, "CAN write interrupted");

      continue;

    } else if ((ret < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {

      /* Our output buffer is full and we're in non-blocking mode.
       * Just silently drop the rest of the output...
       */

      nanosleep((const struct timespec[]){{0, CAN_SLEEP_NS}}, NULL);
      piksi_log(LOG_WARNING, "CAN write buffer full, dropping data");

      return count;

    } else if (ret < 0) {

      /* Some other error happened, pass it up...
       */

      PK_LOG_ANNO(LOG_WARNING, "CAN write failed: %s (%d)", strerror(errno), errno);
      return ret;

    } else {
      return frame.can_dlc;
    }
  }
}

static ssize_t can_write_all(int fd, const void *buffer, size_t count)
{
  uint32_t buffer_index = 0;
  while (buffer_index < count) {
    ssize_t write_count = can_write(fd, &((uint8_t *)buffer)[buffer_index], count - buffer_index);
    if (write_count < 0) {
      return write_count;
    }
    buffer_index += write_count;
  }
  return buffer_index;
}

static void *can_write_thread_handler(void *arg)
{
  piksi_log(LOG_DEBUG, "CAN write thread starting...");
  write_thread_context_t *pctx = (write_thread_context_t *)arg;

  for (;;) {

    ssize_t ret = read(pctx->write_pipe_fd, &pipe_read_buffer, sizeof(pipe_read_buffer));

    if (ret == 0) {
      PK_LOG_ANNO(LOG_DEBUG, "pipe closed");
      return NULL;
    } else if ((ret < -1) && (errno == EINTR)) {
      /* Retry if interrupted */
      continue;
    } else if (ret < -1) {
      PK_LOG_ANNO(LOG_WARNING, "read() indicated failure: %s (%d)", strerror(errno), errno);
      break;
    } else {
      can_write_all(pctx->can_write_fd, pipe_read_buffer, ret);
    }
  }

  piksi_log(LOG_DEBUG, "CAN write thread stopping...");
  return NULL;
}

int can_loop(const char *can_name, u32 can_filter_in)
{
  while (1) {
    /* Open CAN socket */
    struct sockaddr_can addr;
    struct ifreq ifr;
    int socket_can = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    if (socket_can < 0) {
      piksi_log(LOG_ERR, "could not open a socket for %s", can_name);
      return 1;
    }

    can_filter = can_filter_in;

    struct can_filter rfilter[1];
    rfilter[0].can_id = can_filter;
    rfilter[0].can_mask = CAN_SFF_MASK;

    if (setsockopt(socket_can, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter))) {
      piksi_log(LOG_ERR, "could not set filter for %s", can_name);
      return 1;
    }

    const int loopback = 0;

    if (setsockopt(socket_can, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &loopback, sizeof(loopback))) {
      piksi_log(LOG_ERR, "could not disable loopback for %s", can_name);
      return 1;
    }

    strcpy(ifr.ifr_name, can_name);
    if (ioctl(socket_can, SIOCGIFINDEX, &ifr) < 0) {
      piksi_log(LOG_ERR, "could not get index name for %s", can_name);
      return 1;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(socket_can, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      piksi_log(LOG_ERR, "could not bind %s", can_name);
      return 1;
    }

    /* Set socket for blocking */
    int flags = fcntl(socket_can, F_GETFL, 0);
    if (flags < 0) {
      piksi_log(LOG_ERR, "failed CAN flag fetch");
      return 1;
    }

    if (fcntl(socket_can, F_SETFL, flags & ~O_NONBLOCK) < 0) {
      piksi_log(LOG_ERR, "failed CAN flag set");
    }

    int pipe_from_can[2] = {-1, -1};

    if (pipe2(pipe_from_can, O_DIRECT) < 0) {
      piksi_log(LOG_ERR, "failed to create CAN read pipe: %s (%d)", strerror(errno), errno);
      return 1;
    }

    int pipe_to_can[2] = {-1, -1};

    if (pipe2(pipe_to_can, O_DIRECT) < 0) {
      piksi_log(LOG_ERR, "failed to create CAN write pipe: %s (%d)", strerror(errno), errno);
      return 1;
    }

    read_thread_context_t read_thread_ctx = {
      .can_read_fd = socket_can,
      .read_pipe_fd = pipe_from_can[WRITE],
    };

    pthread_t read_thread;
    pthread_create(&read_thread, NULL, can_read_thread_handler, &read_thread_ctx);

    write_thread_context_t write_thread_ctx = {
      .can_write_fd = socket_can,
      .write_pipe_fd = pipe_to_can[READ],
    };

    pthread_t write_thread;
    pthread_create(&write_thread, NULL, can_write_thread_handler, &write_thread_ctx);

    io_loop_run(pipe_from_can[READ], pipe_to_can[WRITE]);

    close(socket_can);

    close(pipe_from_can[READ]);
    close(pipe_from_can[WRITE]);

    close(pipe_to_can[READ]);
    close(pipe_to_can[WRITE]);

    void *thread_ret;

    pthread_join(read_thread, &thread_ret);
    pthread_join(write_thread, &thread_ret);

    socket_can = -1;
  }

  return 0;
}
