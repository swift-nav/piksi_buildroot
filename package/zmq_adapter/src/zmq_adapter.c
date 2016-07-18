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

#include "zmq_adapter.h"
#include "framer.h"

#include <getopt.h>

#define READ_BUFFER_SIZE 65536

typedef enum {
  MODE_INVALID,
  MODE_FILE,
  MODE_TCP_LISTEN
} prog_mode_t;

typedef struct {
  zsock_t *zsock;
  int fd;
} handle_t;

typedef ssize_t (*read_fn_t)(handle_t *handle, void *buffer, size_t count);
typedef ssize_t (*write_fn_t)(handle_t *handle, const void *buffer,
                              size_t count);

static prog_mode_t mode = MODE_INVALID;
static framer_t framer = FRAMER_NONE;

static const char *zmq_pub_addr = NULL;
static const char *zmq_sub_addr = NULL;
static const char *file_path = NULL;
static int tcp_listen_port = -1;

static void usage(char *command)
{
  printf("usage: %s -m <mode> -t <target> [-f <framer>] "
         "[-p zmq_pub_addr] [-s zmq_sub_addr]\n", command);
}

static int parse_options(int argc, char *argv[])
{
  const char *target = NULL;

  int c;
  while ((c = getopt(argc, argv, "m:t:p:s:f:")) != -1) {
    switch (c) {
      case 'm': {
        if (strcmp(optarg, "file") == 0) {
          mode = MODE_FILE;
        } else if (strcmp(optarg, "tcp-l") == 0) {
          mode = MODE_TCP_LISTEN;
        }
      }
      break;

      case 't': {
        target = optarg;
      }
      break;

      case 'p': {
        zmq_pub_addr = optarg;
      }
      break;

      case 's': {
        zmq_sub_addr = optarg;
      }
      break;

      case 'f': {
        if (strcmp(optarg, "sbp") == 0) {
          framer = FRAMER_SBP;
        }
      }
      break;

      default: {
        printf("invalid option\n");
        return -1;
      }
      break;
    }
  }

  if (mode == MODE_INVALID) {
    printf("invalid mode\n");
    return 1;
  }

  if (!zmq_pub_addr && !zmq_sub_addr) {
    printf("ZMQ PUB or SUB not specified\n");
    return 1;
  }

  if (target == NULL) {
    printf("target not specifed\n");
    return 1;
  }

  switch (mode) {
    case MODE_FILE: {
      file_path = target;
    }
    break;

    case MODE_TCP_LISTEN: {
      tcp_listen_port = strtol(target, NULL, 10);
    }
    break;

    default:
      break;
  }

  return 0;
}

static ssize_t zsock_read(zsock_t *zsock, void *buffer, size_t count)
{
  zmsg_t *msg = zmsg_recv(zsock);
  if (msg == NULL) {
    return -1;
  }

  size_t buffer_index = 0;
  zframe_t *frame = zmsg_first(msg);
  while (frame != NULL) {
    const void *data = zframe_data(frame);
    size_t size = zframe_size(frame);

    size_t copy_length = buffer_index + size <= count ?
        size : count - buffer_index;

    if (copy_length > 0) {
      memcpy(&((uint8_t *)buffer)[buffer_index], data, copy_length);
      buffer_index += copy_length;
    }

    frame = zmsg_next(msg);
  }

  zmsg_destroy(&msg);
  assert(msg == NULL);

  return buffer_index;
}

static ssize_t zsock_write(zsock_t *zsock, const void *buffer, size_t count)
{
  int result;

  zmsg_t *msg = zmsg_new();

  result = zmsg_addmem(msg, buffer, count);
  if (result != 0) {
    zmsg_destroy(&msg);
    assert(msg == NULL);
    return -1;
  }

  result = zmsg_send(&msg, zsock);
  if (result != 0) {
    zmsg_destroy(&msg);
    assert(msg == NULL);
    return -1;
  }

  assert(msg == NULL);
  return count;
}

static ssize_t handle_read(handle_t *handle, void *buffer, size_t count)
{
  if (handle->zsock != NULL) {
    return zsock_read(handle->zsock, buffer, count);
  } else {
    return read(handle->fd, buffer, count);
  }
}

static ssize_t handle_write(handle_t *handle, const void *buffer, size_t count)
{
  if (handle->zsock != NULL) {
    return zsock_write(handle->zsock, buffer, count);
  } else {
    return write(handle->fd, buffer, count);
  }
}

static ssize_t handle_write_all(handle_t *handle,
                                const void *buffer, size_t count)
{
  uint32_t buffer_index = 0;
  while (buffer_index < count) {
    ssize_t write_count = handle_write(handle,
                                       &((uint8_t *)buffer)[buffer_index],
                                       count - buffer_index);
    printf("wrote %zd bytes\n", write_count);
    if (write_count <= 0) {
      return write_count;
    }
    buffer_index += write_count;
  }
  return buffer_index;
}

static ssize_t handle_write_all_via_framer(handle_t *handle,
                                           const void *buffer, size_t count,
                                           framer_t framer,
                                           framer_state_t *framer_state)
{
  /* Pass data through framer */
  uint32_t buffer_index = 0;
  while (buffer_index < count) {
    const uint8_t *frame;
    uint32_t frame_length;
    buffer_index +=
        framer_process(framer, framer_state,
                       &((uint8_t *)buffer)[buffer_index],
                       count - buffer_index,
                       &frame, &frame_length);
    if (frame == NULL) {
      continue;
    }

    printf("decoded frame\n");

    /* Write frame to handle */
    ssize_t write_count = handle_write_all(handle, frame, frame_length);
    if (write_count <= 0) {
      return write_count;
    }
    if (write_count != frame_length) {
      printf("warning: write_count != frame_length\n");
    }

  }
  return buffer_index;
}

static void io_loop(handle_t *read_handle, handle_t *write_handle,
                    framer_t framer)
{
  printf("io loop begin\n");

  framer_state_t framer_state;
  framer_init(framer, &framer_state);

  while (1) {
    /* Read from read_handle */
    uint8_t buffer[READ_BUFFER_SIZE];
    ssize_t read_count = handle_read(read_handle, buffer, sizeof(buffer));
    printf("read %zd bytes\n", read_count);
    if (read_count <= 0) {
      break;
    }

    /* Write to write handle via framer */
    ssize_t write_count = handle_write_all_via_framer(write_handle,
                                                      buffer, read_count,
                                                      framer, &framer_state);
    if (write_count <= 0) {
      break;
    }
    if (write_count != read_count) {
      printf("warning: write_count != read_count\n");
    }
  }

  printf("io loop end\n");
}

void io_loop_start(int fd)
{
  /* TODO: switch on ZSOCK type */

  if (zmq_pub_addr != NULL) {
    if (fork() == 0) {
      /* child process */
      zsock_t *pub = zsock_new_pub(zmq_pub_addr);
      if (pub != NULL) {
        printf("opened PUB socket: %s\n", zmq_pub_addr);
        handle_t pub_handle = {.zsock = pub, .fd = -1};
        handle_t fd_handle = {.zsock = NULL, .fd = fd};
        io_loop(&fd_handle, &pub_handle, framer);
        zsock_destroy(&pub);
        assert(pub == NULL);
      } else {
        printf("error opening PUB socket: %s\n", zmq_pub_addr);
      }
      return;
    }
  }

  if (zmq_sub_addr != NULL) {
    if (fork() == 0) {
      /* child process */
      zsock_t *sub = zsock_new_sub(zmq_sub_addr, "");
      if (sub != NULL) {
        printf("opened SUB socket: %s\n", zmq_sub_addr);
        handle_t sub_handle = {.zsock = sub, .fd = -1};
        handle_t fd_handle = {.zsock = NULL, .fd = fd};
        /* SUB loop should never need a framer */
        io_loop(&sub_handle, &fd_handle, FRAMER_NONE);
        zsock_destroy(&sub);
        assert(sub == NULL);
      } else {
        printf("error opening SUB socket: %s\n", zmq_sub_addr);
      }
      return;
    }
  }
}

int main(int argc, char *argv[])
{
  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(1);
  }

  signal(SIGCHLD, SIG_IGN); /* Automatically reap child processes */
  signal(SIGPIPE, SIG_IGN); /* Allow write to return an error */

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  int ret = 0;

  switch (mode) {
    case MODE_FILE: {
      extern int file_loop(const char *file_path);
      ret = file_loop(file_path);
    }
    break;

    case MODE_TCP_LISTEN: {
      extern int tcp_listen_loop(int port);
      ret = tcp_listen_loop(tcp_listen_port);
    }
    break;

    default:
      break;
  }

  return ret;
}
