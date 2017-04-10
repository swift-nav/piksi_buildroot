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
#include "filter.h"
#include "protocols.h"
#include <getopt.h>
#include <dlfcn.h>

#define PROTOCOL_LIBRARY_PATH "/usr/lib/zmq_protocols"
#define READ_BUFFER_SIZE 65536
#define REP_TIMEOUT_DEFAULT_ms 10000
#define ZSOCK_RESTART_RETRY_COUNT 3
#define ZSOCK_RESTART_RETRY_DELAY_ms 1
#define FRAMER_NONE_NAME "none"
#define FILTER_NONE_NAME "none"

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
  framer_t *framer;
  filter_t *filter;
} handle_t;

typedef ssize_t (*read_fn_t)(handle_t *handle, void *buffer, size_t count);
typedef ssize_t (*write_fn_t)(handle_t *handle, const void *buffer,
                              size_t count);

static bool debug = false;
static io_mode_t io_mode = IO_INVALID;
static zsock_mode_t zsock_mode = ZSOCK_INVALID;
static const char *framer_name = FRAMER_NONE_NAME;
static const char *filter_in_name = FRAMER_NONE_NAME;
static const char *filter_out_name = FRAMER_NONE_NAME;
static const char *filter_in_config = NULL;
static const char *filter_out_config = NULL;
static int rep_timeout_ms = REP_TIMEOUT_DEFAULT_ms;

static const char *zmq_pub_addr = NULL;
static const char *zmq_sub_addr = NULL;
static const char *zmq_req_addr = NULL;
static const char *zmq_rep_addr = NULL;
static const char *file_path = NULL;
static int tcp_listen_port = -1;

static void debug_printf(const char *msg, ...)
{
  if (!debug) {
    return;
  }

  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);
}

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

  puts("\nFilter Mode - optional");
  puts("\t--filter-in <filter>");
  puts("\t--filter-out <filter>");
  puts("\t--filter-in-config <file>");
  puts("\t--filter-out-config <file>");
  puts("\t\tfilter configuration file");

  puts("\nIO Modes - select one");
  puts("\t--file <file>");
  puts("\t--tcp-l <port>");

  puts("\nMisc options");
  puts("\t--rep-timeout <ms>");
  puts("\t\tresponse timeout before resetting a REP socket");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_FILE = 1,
    OPT_ID_TCP_LISTEN,
    OPT_ID_REP_TIMEOUT,
    OPT_ID_DEBUG,
    OPT_ID_FILTER_IN,
    OPT_ID_FILTER_OUT,
    OPT_ID_FILTER_IN_CONFIG,
    OPT_ID_FILTER_OUT_CONFIG
  };

  const struct option long_opts[] = {
    {"pub",               required_argument, 0, 'p'},
    {"sub",               required_argument, 0, 's'},
    {"req",               required_argument, 0, 'r'},
    {"rep",               required_argument, 0, 'y'},
    {"framer",            required_argument, 0, 'f'},
    {"file",              required_argument, 0, OPT_ID_FILE},
    {"tcp-l",             required_argument, 0, OPT_ID_TCP_LISTEN},
    {"rep-timeout",       required_argument, 0, OPT_ID_REP_TIMEOUT},
    {"filter-in",         required_argument, 0, OPT_ID_FILTER_IN},
    {"filter-out",        required_argument, 0, OPT_ID_FILTER_OUT},
    {"filter-in-config",  required_argument, 0, OPT_ID_FILTER_IN_CONFIG},
    {"filter-out-config", required_argument, 0, OPT_ID_FILTER_OUT_CONFIG},
    {"debug",             no_argument,       0, OPT_ID_DEBUG},
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

      case OPT_ID_REP_TIMEOUT: {
        rep_timeout_ms = strtol(optarg, NULL, 10);
      }
      break;

      case OPT_ID_FILTER_IN: {
        if (filter_interface_valid(optarg) == 0) {
          filter_in_name = optarg;
        } else {
          printf("invalid input filter\n");
          return -1;
        }
      }
      break;

      case OPT_ID_FILTER_OUT: {
        if (filter_interface_valid(optarg) == 0) {
          filter_out_name = optarg;
        } else {
          printf("invalid output filter\n");
          return -1;
        }
      }
      break;

      case OPT_ID_FILTER_IN_CONFIG: {
        filter_in_config = optarg;
      }
      break;

      case OPT_ID_FILTER_OUT_CONFIG: {
        filter_out_config = optarg;
      }
      break;

      case OPT_ID_DEBUG: {
        debug = true;
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
        if (framer_interface_valid(optarg) == 0) {
          framer_name = optarg;
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
    return -1;
  }

  if (zsock_mode == ZSOCK_INVALID) {
    printf("ZMQ address(es) not specified\n");
    return -1;
  }

  if ((strcasecmp(filter_in_name, FILTER_NONE_NAME) == 0) !=
      (filter_in_config == NULL)) {
    printf("invalid input filter settings\n");
    return -1;
  }

  if ((strcasecmp(filter_out_name, FILTER_NONE_NAME) == 0) !=
      (filter_out_config == NULL)) {
    printf("invalid output filter settings\n");
    return -1;
  }

  return 0;
}

static void sigchld_handler(int signum)
{
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0) {
    ;
  }
  errno = saved_errno;
}

static void terminate_handler(int signum)
{
  /* Send this signal to the entire process group */
  killpg(0, signum);

  /* Exit */
  _exit(EXIT_SUCCESS);
}

static void handle_deinit(handle_t *handle)
{
  if (handle->framer != NULL) {
    framer_destroy(&handle->framer);
    assert(handle->framer == NULL);
  }

  if (handle->filter != NULL) {
    filter_destroy(&handle->filter);
    assert(handle->filter == NULL);
  }
}

static int handle_init(handle_t *handle, zsock_t *zsock, int fd,
                       const char *framer_name, const char *filter_name,
                       const char *filter_config)
{
  *handle = (handle_t) {
    .zsock = zsock,
    .fd = fd,
    .framer = framer_create(framer_name),
    .filter = filter_create(filter_name, filter_config)
  };

  if ((handle->framer == NULL) || (handle->filter == NULL)) {
    handle_deinit(handle);
    return -1;
  }

  return 0;
}

static zmq_pollitem_t handle_to_pollitem(const handle_t *handle, short events)
{
  zmq_pollitem_t pollitem = {
    .socket = handle->zsock == NULL ? NULL : zsock_resolve(handle->zsock),
    .fd = handle->fd,
    .events = events
  };
  return pollitem;
}

static zsock_t * zsock_start(int type)
{
  zsock_t *zsock = zsock_new(type);
  if (zsock == NULL) {
    return zsock;
  }

  /* Set any type-specific options and get address */
  const char *addr = NULL;
  bool serverish = false;
  switch (type) {
    case ZMQ_PUB: {
      addr = zmq_pub_addr;
      serverish = true;
    }
    break;

    case ZMQ_SUB: {
      addr = zmq_sub_addr;
      serverish = false;
      zsock_set_subscribe(zsock, "");
    }
    break;

    case ZMQ_REQ: {
      addr = zmq_req_addr;
      serverish = false;
      zsock_set_req_relaxed(zsock, 1);
      zsock_set_req_correlate(zsock, 1);
    }
    break;

    case ZMQ_REP: {
      addr = zmq_rep_addr;
      serverish = true;
    }
    break;

    default: {
      printf("unknown socket type\n");
    }
    break;
  }

  if (zsock_attach(zsock, addr, serverish) != 0) {
    printf("error opening socket: %s\n", addr);
    zsock_destroy(&zsock);
    assert(zsock == NULL);
    return zsock;
  }

  debug_printf("opened socket: %s\n", addr);
  return zsock;
}

static void zsock_restart(zsock_t **p_zsock)
{
  int type = zsock_type(*p_zsock);
  zsock_destroy(p_zsock);
  assert(*p_zsock == NULL);

  /* Closing a bound socket can take some time.
   * Try a few times to reopen. */
  int retry = ZSOCK_RESTART_RETRY_COUNT;
  do {
    usleep(1000 * ZSOCK_RESTART_RETRY_DELAY_ms);
    *p_zsock = zsock_start(type);
  } while ((*p_zsock == NULL) && (--retry > 0));
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

static ssize_t fd_read(int fd, void *buffer, size_t count)
{
  while (1) {
    ssize_t ret = read(fd, buffer, count);
    /* Retry if interrupted */
    if ((ret == -1) && (errno == EINTR)) {
      continue;
    } else {
      return ret;
    }
  }
}

static ssize_t fd_write(int fd, const void *buffer, size_t count)
{
  while (1) {
    ssize_t ret = write(fd, buffer, count);
    /* Retry if interrupted */
    if ((ret == -1) && (errno == EINTR)) {
      continue;
    } else {
      return ret;
    }
  }
}

static ssize_t handle_read(handle_t *handle, void *buffer, size_t count)
{
  if (handle->zsock != NULL) {
    return zsock_read(handle->zsock, buffer, count);
  } else {
    return fd_read(handle->fd, buffer, count);
  }
}

static ssize_t handle_write(handle_t *handle, const void *buffer, size_t count)
{
  if (handle->zsock != NULL) {
    return zsock_write(handle->zsock, buffer, count);
  } else {
    return fd_write(handle->fd, buffer, count);
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
    debug_printf("wrote %zd bytes\n", write_count);
    if (write_count <= 0) {
      return write_count;
    }
    buffer_index += write_count;
  }
  return buffer_index;
}

static ssize_t handle_write_one_via_framer(handle_t *handle,
                                           const void *buffer, size_t count,
                                           size_t *frames_written)
{
  /* Pass data through framer */
  *frames_written = 0;
  uint32_t buffer_index = 0;
  while (buffer_index < count) {
    const uint8_t *frame;
    uint32_t frame_length;
    buffer_index +=
        framer_process(handle->framer,
                       &((uint8_t *)buffer)[buffer_index],
                       count - buffer_index,
                       &frame, &frame_length);
    if (frame == NULL) {
      continue;
    }


    /* Pass frame through filter */
    if (filter_process(handle->filter, frame, frame_length) != 0) {
      debug_printf("ignoring frame\n");
      continue;
    }

    *frames_written += 1;

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
                                           size_t *frames_written)
{
  *frames_written = 0;
  uint32_t buffer_index = 0;
  while (buffer_index < count) {
    size_t frames;
    ssize_t write_count =
        handle_write_one_via_framer(handle,
                                    &((uint8_t *)buffer)[buffer_index],
                                    count - buffer_index,
                                    &frames);
    if (write_count <= 0) {
      return write_count;
    }

    *frames_written += frames;
    buffer_index += write_count;
  }
  return buffer_index;
}

static ssize_t frame_transfer(handle_t *read_handle, handle_t *write_handle,
                              bool *success)
{
  *success = false;

  /* Read from read_handle */
  uint8_t buffer[READ_BUFFER_SIZE];
  ssize_t read_count = handle_read(read_handle, buffer, sizeof(buffer));
  debug_printf("read %zd bytes\n", read_count);
  if (read_count <= 0) {
    return read_count;
  }

  /* Write to write_handle via framer */
  size_t frames_written;
  ssize_t write_count = handle_write_one_via_framer(write_handle,
                                                    buffer, read_count,
                                                    &frames_written);
  if (write_count <= 0) {
    return write_count;
  }
  if (write_count != read_count) {
    printf("warning: write_count != read_count\n");
  }

  *success = (frames_written == 1);
  return write_count;
}

static void io_loop_pubsub(handle_t *read_handle, handle_t *write_handle)
{
  debug_printf("io loop begin\n");

  while (1) {
    /* Read from read_handle */
    uint8_t buffer[READ_BUFFER_SIZE];
    ssize_t read_count = handle_read(read_handle, buffer, sizeof(buffer));
    debug_printf("read %zd bytes\n", read_count);
    if (read_count <= 0) {
      break;
    }

    /* Write to write_handle via framer */
    ssize_t frames_written;
    ssize_t write_count = handle_write_all_via_framer(write_handle,
                                                      buffer, read_count,
                                                      &frames_written);
    if (write_count <= 0) {
      break;
    }
    if (write_count != read_count) {
      printf("warning: write_count != read_count\n");
    }
  }

  debug_printf("io loop end\n");
}

static void io_loop_reqrep(handle_t *req_handle, handle_t *rep_handle)
{
  debug_printf("io loop begin\n");

  int poll_timeout_ms = rep_handle->zsock != NULL ? rep_timeout_ms : -1;
  bool reply_pending = false;

  while (1) {
    enum {
      POLLITEM_REQ,
      POLLITEM_REP,
      POLLITEM__COUNT
    };

    zmq_pollitem_t pollitems[] = {
      [POLLITEM_REQ] = handle_to_pollitem(req_handle, ZMQ_POLLIN),
      [POLLITEM_REP] = handle_to_pollitem(rep_handle, ZMQ_POLLIN),
    };

    int poll_ret = zmq_poll(pollitems, POLLITEM__COUNT, poll_timeout_ms);
    if ((poll_ret == -1) && (errno == EINTR)) {
      /* Retry if interrupted */
      continue;
    } else if (poll_ret < 0) {
      /* Break on error */
      break;
    }

    if (poll_ret == 0) {
      /* Timeout */
      if ((rep_handle->zsock != NULL) && reply_pending) {
        /* Assume the outstanding request was lost.
         * Reset the REP socket so that another request may be received. */
        printf("reply timeout - resetting socket\n");
        zsock_restart(&rep_handle->zsock);
        if (rep_handle->zsock == NULL) {
          break;
        }
        reply_pending = false;
      }
      continue;
    }

    /* Check req_handle */
    if (pollitems[POLLITEM_REQ].revents & ZMQ_POLLIN) {
      if (!reply_pending) {
        printf("warning: reply received but not pending\n");
        if (rep_handle->zsock != NULL) {
          /* Reply received with no request outstanding.
           * Read and drop data from req_handle. */
          printf("dropping data\n");
          uint8_t buffer[READ_BUFFER_SIZE];
          ssize_t read_count = handle_read(req_handle, buffer, sizeof(buffer));
          debug_printf("read %zd bytes\n", read_count);
          if (read_count <= 0) {
            break;
          }

          continue;
        }
      }

      bool ok;
      if (frame_transfer(req_handle, rep_handle, &ok) <= 0) {
        break;
      }

      if (ok) {
        reply_pending = false;
      }
    }

    /* Check rep_handle */
    if (pollitems[POLLITEM_REP].revents & ZMQ_POLLIN) {
      if (reply_pending) {
        printf("warning: request received while already pending\n");
        if (req_handle->zsock != NULL) {
          /* Request received with another outstanding.
           * Reset the REQ socket so that the new request may be sent. */
          printf("resetting socket\n");
          zsock_restart(&req_handle->zsock);
          if (req_handle->zsock == NULL) {
            break;
          }
          reply_pending = false;
        }
      }

      bool ok;
      if (frame_transfer(rep_handle, req_handle, &ok) <= 0) {
        break;
      }

      if (ok) {
        reply_pending = true;
      }
    }
  }

  debug_printf("io loop end\n");
}

void io_loop_start(int fd)
{
  switch (zsock_mode) {
    case ZSOCK_PUBSUB: {

      if (zmq_pub_addr != NULL) {
        if (fork() == 0) {
          /* child process */
          zsock_t *pub = zsock_start(ZMQ_PUB);
          if (pub == NULL) {
            exit(EXIT_FAILURE);
          }

          /* Read from fd, write to pub */
          handle_t pub_handle;
          if (handle_init(&pub_handle, pub, -1, framer_name,
                          filter_in_name, filter_in_config) != 0) {
            exit(EXIT_FAILURE);
          }

          handle_t fd_handle;
          if (handle_init(&fd_handle, NULL, fd, FRAMER_NONE_NAME,
                          FILTER_NONE_NAME, NULL) != 0) {
            exit(EXIT_FAILURE);
          }

          io_loop_pubsub(&fd_handle, &pub_handle);

          zsock_destroy(&pub);
          assert(pub == NULL);
          handle_deinit(&pub_handle);
          handle_deinit(&fd_handle);
          exit(EXIT_SUCCESS);
        }
      }

      if (zmq_sub_addr != NULL) {
        if (fork() == 0) {
          /* child process */
          zsock_t *sub = zsock_start(ZMQ_SUB);
          if (sub == NULL) {
            exit(EXIT_FAILURE);
          }

          /* Read from sub, write to fd */
          handle_t sub_handle;
          if (handle_init(&sub_handle, sub, -1, FRAMER_NONE_NAME,
                          FILTER_NONE_NAME, NULL) != 0) {
            exit(EXIT_FAILURE);
          }

          handle_t fd_handle;
          if (handle_init(&fd_handle, NULL, fd, FRAMER_NONE_NAME,
                          filter_out_name, filter_out_config) != 0) {
            exit(EXIT_FAILURE);
          }

          io_loop_pubsub(&sub_handle, &fd_handle);

          zsock_destroy(&sub);
          assert(sub == NULL);
          handle_deinit(&sub_handle);
          handle_deinit(&fd_handle);
          exit(EXIT_SUCCESS);
        }
      }

    }
    break;

    case ZSOCK_REQ: {

      if (fork() == 0) {
        /* child process */
        zsock_t *req = zsock_start(ZMQ_REQ);
        if (req == NULL) {
          exit(EXIT_FAILURE);
        }

        handle_t req_handle;
        if (handle_init(&req_handle, req, -1, framer_name,
                        filter_in_name, filter_in_config) != 0) {
          exit(EXIT_FAILURE);
        }

        handle_t fd_handle;
        if (handle_init(&fd_handle, NULL, fd, FRAMER_NONE_NAME,
                        filter_out_name, filter_out_config) != 0) {
          exit(EXIT_FAILURE);
        }

        io_loop_reqrep(&req_handle, &fd_handle);

        zsock_destroy(&req);
        assert(req == NULL);
        handle_deinit(&req_handle);
        handle_deinit(&fd_handle);
        exit(EXIT_SUCCESS);
      }

    }
    break;

    case ZSOCK_REP: {

      if (fork() == 0) {
        /* child process */
        zsock_t *rep = zsock_start(ZMQ_REP);
        if (rep == NULL) {
          exit(EXIT_FAILURE);
        }

        handle_t rep_handle;
        if (handle_init(&rep_handle, rep, -1, framer_name,
                        filter_in_name, filter_in_config) != 0) {
          exit(EXIT_FAILURE);
        }

        handle_t fd_handle;
        if (handle_init(&fd_handle, NULL, fd, FRAMER_NONE_NAME,
                        filter_out_name, filter_out_config) != 0) {
          exit(EXIT_FAILURE);
        }

        io_loop_reqrep(&fd_handle, &rep_handle);

        zsock_destroy(&rep);
        assert(rep == NULL);
        handle_deinit(&rep_handle);
        handle_deinit(&fd_handle);
        exit(EXIT_SUCCESS);
      }

    }
    break;

    default:
      break;
  }
}

int main(int argc, char *argv[])
{
  setpgid(0, 0); /* Set PGID = PID */

  if (protocols_import(PROTOCOL_LIBRARY_PATH) != 0) {
    printf("error importing protocols\n");
    exit(EXIT_FAILURE);
  }

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(1);
  }

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  signal(SIGPIPE, SIG_IGN); /* Allow write to return an error */

  /* Set up SIGCHLD handler */
  struct sigaction sigchld_sa;
  sigchld_sa.sa_handler = sigchld_handler;
  sigemptyset(&sigchld_sa.sa_mask);
  sigchld_sa.sa_flags = SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sigchld_sa, NULL) != 0) {
    printf("error setting up sigchld handler\n");
    exit(EXIT_FAILURE);
  }

  /* Set up handler for signals which should terminate the program */
  struct sigaction terminate_sa;
  terminate_sa.sa_handler = terminate_handler;
  sigemptyset(&terminate_sa.sa_mask);
  terminate_sa.sa_flags = 0;
  if ((sigaction(SIGINT, &terminate_sa, NULL) != 0) ||
      (sigaction(SIGTERM, &terminate_sa, NULL) != 0) ||
      (sigaction(SIGQUIT, &terminate_sa, NULL) != 0)) {
    printf("error setting up terminate handler\n");
    exit(EXIT_FAILURE);
  }

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

  raise(SIGINT);
  return ret;
}
