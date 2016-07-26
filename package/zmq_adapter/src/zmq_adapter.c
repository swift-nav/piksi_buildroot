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
  IO_INVALID,
  IO_FILE,
  IO_TCP_LISTEN
} io_mode_t;

typedef enum {
  ZSOCK_INVALID,
  ZSOCK_PUBSUB,
  ZSOCK_REQ,
  ZSOCK_REP
} zsock_mode_t;

typedef struct {
  zsock_t *zsock;
  int fd;
} handle_t;

typedef ssize_t (*read_fn_t)(handle_t *handle, void *buffer, size_t count);
typedef ssize_t (*write_fn_t)(handle_t *handle, const void *buffer,
                              size_t count);

static io_mode_t io_mode = IO_INVALID;
static zsock_mode_t zsock_mode = ZSOCK_INVALID;
static framer_t framer = FRAMER_NONE;

static const char *zmq_pub_addr = NULL;
static const char *zmq_sub_addr = NULL;
static const char *zmq_req_addr = NULL;
static const char *zmq_rep_addr = NULL;
static const char *file_path = NULL;
static int tcp_listen_port = -1;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nZMQ Modes - select one or two (see notes)");
  puts("\t-p, --pub <addr>");
  puts("\t\tsink socket, may be combined with --sub");
  puts("\t-s, --sub <addr>");
  puts("\t\tsource socket, may be combined with --pub");
  puts("\t-r, --req <addr>");
  puts("\t\tbidir socket, may not be combined");
  puts("\t-y, --rep <addr>");
  puts("\t\tbidir socket, may not be combined");

  puts("\nFramer Mode - optional");
  puts("\t-f, --framer <framer>");
  puts("\t\tavailable framers: sbp");

  puts("\nIO Modes - select one");
  puts("\t--file <file>");
  puts("\t--tcp-l <port>");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_FILE = 1,
    OPT_ID_TCP_LISTEN,
  };

  const struct option long_opts[] = {
    {"pub",     required_argument, 0, 'p'},
    {"sub",     required_argument, 0, 's'},
    {"req",     required_argument, 0, 'r'},
    {"rep",     required_argument, 0, 'y'},
    {"framer",  required_argument, 0, 'f'},
    {"file",    required_argument, 0, OPT_ID_FILE},
    {"tcp-l",   required_argument, 0, OPT_ID_TCP_LISTEN},
    {0, 0, 0, 0}
  };

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "p:s:r:y:f:",
                          long_opts, &opt_index)) != -1) {
    switch (c) {
      case OPT_ID_FILE: {
        io_mode = IO_FILE;
        file_path = optarg;
      }
      break;

      case OPT_ID_TCP_LISTEN: {
        io_mode = IO_TCP_LISTEN;
        tcp_listen_port = strtol(optarg, NULL, 10);
      }
      break;

      case 'p': {
        zsock_mode = ZSOCK_PUBSUB;
        zmq_pub_addr = optarg;
      }
      break;

      case 's': {
        zsock_mode = ZSOCK_PUBSUB;
        zmq_sub_addr = optarg;
      }
      break;

      case 'r': {
        zsock_mode = ZSOCK_REQ;
        zmq_req_addr = optarg;
      }
      break;

      case 'y': {
        zsock_mode = ZSOCK_REP;
        zmq_rep_addr = optarg;
      }
      break;

      case 'f': {
        if (strcasecmp(optarg, "SBP") == 0) {
          framer = FRAMER_SBP;
        } else {
          printf("invalid framer\n");
          return -1;
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

  if (io_mode == IO_INVALID) {
    printf("invalid mode\n");
    return 1;
  }

  if (zsock_mode == ZSOCK_INVALID) {
    printf("ZMQ address(es) not specified\n");
    return 1;
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

static ssize_t handle_write_one_via_framer(handle_t *handle,
                                           const void *buffer, size_t count,
                                           framer_state_t *framer_state)
{
  /* Pass data through framer */
  uint32_t buffer_index = 0;
  while (buffer_index < count) {
    const uint8_t *frame;
    uint32_t frame_length;
    buffer_index +=
        framer_process(framer_state,
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

    return buffer_index;
  }
  return buffer_index;
}

static ssize_t handle_write_all_via_framer(handle_t *handle,
                                           const void *buffer, size_t count,
                                           framer_state_t *framer_state)
{
  uint32_t buffer_index = 0;
  while (buffer_index < count) {
    ssize_t write_count =
        handle_write_one_via_framer(handle,
                                    &((uint8_t *)buffer)[buffer_index],
                                    count - buffer_index,
                                    framer_state);
    if (write_count <= 0) {
      return write_count;
    }

    buffer_index += write_count;
  }
  return buffer_index;
}

static void io_loop_pubsub(handle_t *read_handle, handle_t *write_handle,
                           framer_t framer)
{
  printf("io loop begin\n");

  framer_state_t framer_state;
  framer_state_init(&framer_state, framer);

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
                                                      &framer_state);
    if (write_count <= 0) {
      break;
    }
    if (write_count != read_count) {
      printf("warning: write_count != read_count\n");
    }
  }

  printf("io loop end\n");
}

static ssize_t frame_transfer(handle_t *read_handle, handle_t *write_handle,
                              framer_state_t *framer_state)
{
  while (1) {
    /* Read from read_handle */
    uint8_t buffer[READ_BUFFER_SIZE];
    ssize_t read_count = handle_read(read_handle, buffer, sizeof(buffer));
    printf("read %zd bytes\n", read_count);
    if (read_count <= 0) {
      return read_count;
    }

    /* Write to write handle via framer */
    ssize_t write_count = handle_write_one_via_framer(write_handle,
                                                      buffer, read_count,
                                                      framer_state);
    if (write_count <= 0) {
      return read_count;
    }
    if (write_count != read_count) {
      printf("warning: write_count != read_count\n");
    }

    return write_count;
  }
}

static void io_loop_reqrep(handle_t *outer_handle, framer_t outer_framer,
                           handle_t *inner_handle, framer_t inner_framer)
{
  printf("io loop begin\n");

  framer_state_t outer_framer_state;
  framer_state_init(&outer_framer_state, outer_framer);
  framer_state_t inner_framer_state;
  framer_state_init(&inner_framer_state, inner_framer);

  while (1) {
    /* Transfer one frame from outer_handle to inner_handle */
    if (frame_transfer(outer_handle, inner_handle, &outer_framer_state) <= 0) {
      break;
    }

    /* Transfer one frame from inner_handle to outer_handle */
    if (frame_transfer(inner_handle, outer_handle, &inner_framer_state) <= 0) {
      break;
    }
  }

  printf("io loop end\n");
}

void io_loop_start(int fd)
{
  switch (zsock_mode) {
    case ZSOCK_PUBSUB: {

      if (zmq_pub_addr != NULL) {
        if (fork() == 0) {
          /* child process */
          zsock_t *pub = zsock_new_pub(zmq_pub_addr);
          if (pub != NULL) {
            printf("opened PUB socket: %s\n", zmq_pub_addr);
            handle_t pub_handle = {.zsock = pub, .fd = -1};
            handle_t fd_handle = {.zsock = NULL, .fd = fd};
            io_loop_pubsub(&fd_handle, &pub_handle, framer);
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
            io_loop_pubsub(&sub_handle, &fd_handle, FRAMER_NONE);
            zsock_destroy(&sub);
            assert(sub == NULL);
          } else {
            printf("error opening SUB socket: %s\n", zmq_sub_addr);
          }
          return;
        }
      }

    }
    break;

    case ZSOCK_REQ: {

      if (fork() == 0) {
        /* child process */
        zsock_t *req = zsock_new_req(zmq_req_addr);
        if (req != NULL) {
          printf("opened REQ socket: %s\n", zmq_req_addr);
          handle_t req_handle = {.zsock = req, .fd = -1};
          handle_t fd_handle = {.zsock = NULL, .fd = fd};
          io_loop_reqrep(&fd_handle, framer, &req_handle, FRAMER_NONE);
          zsock_destroy(&req);
          assert(req == NULL);
        } else {
          printf("error opening REQ socket: %s\n", zmq_req_addr);
        }
        return;
      }

    }
    break;

    case ZSOCK_REP: {

      if (fork() == 0) {
        /* child process */
        zsock_t *rep = zsock_new_rep(zmq_rep_addr);
        if (rep != NULL) {
          printf("opened REP socket: %s\n", zmq_rep_addr);
          handle_t rep_handle = {.zsock = rep, .fd = -1};
          handle_t fd_handle = {.zsock = NULL, .fd = fd};
          io_loop_reqrep(&rep_handle, FRAMER_NONE, &fd_handle, framer);
          zsock_destroy(&rep);
          assert(rep == NULL);
        } else {
          printf("error opening REP socket: %s\n", zmq_rep_addr);
        }
        return;
      }

    }
    break;

    default:
      break;
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

  switch (io_mode) {
    case IO_FILE: {
      extern int file_loop(const char *file_path);
      ret = file_loop(file_path);
    }
    break;

    case IO_TCP_LISTEN: {
      extern int tcp_listen_loop(int port);
      ret = tcp_listen_loop(tcp_listen_port);
    }
    break;

    default:
      break;
  }

  return ret;
}
