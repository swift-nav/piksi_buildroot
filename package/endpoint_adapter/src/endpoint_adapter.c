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

#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <getopt.h>
#include <dlfcn.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>

#include <libpiksi/logging.h>
#include <libpiksi/loop.h>
#include <libpiksi/util.h>
#include <libpiksi/metrics.h>

#include "endpoint_adapter.h"
#include "framer.h"
#include "filter.h"
#include "protocols.h"

#define PROTOCOL_LIBRARY_PATH_ENV_NAME "PROTOCOL_LIBRARY_PATH"
#define PROTOCOL_LIBRARY_PATH_DEFAULT "/usr/lib/endpoint_protocols"
#define READ_BUFFER_SIZE 65536
#define REP_TIMEOUT_DEFAULT_ms 10000
#define STARTUP_DELAY_DEFAULT_ms 0
#define ENDPOINT_RESTART_RETRY_COUNT 3
#define ENDPOINT_RESTART_RETRY_DELAY_ms 1
#define FRAMER_NONE_NAME "none"
#define FILTER_NONE_NAME "none"

#define PROGRAM_NAME "endpoint_adapter"

#define MI metrics_indexes
#define MT metrics_table
#define MR metrics_ref

static const u64 one_second_ns = 1e9;
static u64 last_metrics_flush = 0;

static void do_metrics_flush(void);
static void setup_metrics(const char* pubsub);

static pk_metrics_t* MR;

PK_METRICS_TABLE(MT, MI,

  PK_METRICS_ENTRY("read/count",           "per_second",    M_U32,        M_UPDATE_COUNT,   M_RESET_DEF, read_count),
  PK_METRICS_ENTRY("read/size/per_second", "total",         M_U32,        M_UPDATE_SUM,     M_RESET_DEF, read_size_total),
  PK_METRICS_ENTRY("read/size/per_second", "average",       M_U32,        M_UPDATE_AVERAGE, M_RESET_DEF, read_size_average,
                   M_AVERAGE_OF(MI,        read_size_total, read_count)),
  PK_METRICS_ENTRY("error/mismatch",       "total",         M_U32,        M_UPDATE_COUNT,   M_RESET_DEF, mismatch)
)

typedef enum {
  IO_INVALID,
  IO_STDIO,
  IO_FILE,
  IO_TCP_LISTEN,
  IO_TCP_CONNECT,
  IO_UDP_LISTEN,
  IO_UDP_CONNECT
} io_mode_t;

typedef enum {
  ENDPOINT_INVALID,
  ENDPOINT_PUBSUB,
} endpoint_mode_t;

typedef struct {
  pk_endpoint_t *pk_ept;
  int read_fd;
  int write_fd;
  framer_t *framer;
  filter_t *filter;
} handle_t;

typedef ssize_t (*read_fn_t)(handle_t *handle, void *buffer, size_t count);
typedef ssize_t (*write_fn_t)(handle_t *handle, const void *buffer,
                              size_t count);

bool debug = false;
static io_mode_t io_mode = IO_INVALID;
static endpoint_mode_t endpoint_mode = ENDPOINT_INVALID;
static const char *framer_name = FRAMER_NONE_NAME;
static const char *filter_in_name = FRAMER_NONE_NAME;
static const char *filter_out_name = FRAMER_NONE_NAME;
static const char *filter_in_config = NULL;
static const char *filter_out_config = NULL;
static int startup_delay_ms = STARTUP_DELAY_DEFAULT_ms;
static bool nonblock = false;
static int outq;

static const char *pub_addr = NULL;
static const char *sub_addr = NULL;
static const char *port_name = "<unknown>";
static char file_path[PATH_MAX] = "";
static int tcp_listen_port = -1;
static const char *tcp_connect_addr = NULL;
static int udp_listen_port = -1;
static const char *udp_connect_addr = NULL;

static pid_t pub_pid = -1;
static pid_t sub_pid = -1;
static pid_t *pids[] = {
  &pub_pid,
  &sub_pid,
};

static void usage(char *command)
{
  fprintf(stderr, "Usage: %s\n", command);

  fprintf(stderr, "\nEndpoint Modes - select one or two (see notes)\n");
  fprintf(stderr, "\t-p, --pub <addr>\n");
  fprintf(stderr, "\t\tsink socket, may be combined with --sub\n");
  fprintf(stderr, "\t-s, --sub <addr>\n");
  fprintf(stderr, "\t\tsource socket, may be combined with --pub\n");

  fprintf(stderr, "\nFramer Mode - optional\n");
  fprintf(stderr, "\t-f, --framer <framer>\n");

  fprintf(stderr, "\nFilter Mode - optional\n");
  fprintf(stderr, "\t--filter-in <filter>\n");
  fprintf(stderr, "\t--filter-out <filter>\n");
  fprintf(stderr, "\t--filter-in-config <file>\n");
  fprintf(stderr, "\t--filter-out-config <file>\n");
  fprintf(stderr, "\t\tfilter configuration file\n");

  fprintf(stderr, "\nIO Modes - select one\n");
  fprintf(stderr, "\t--stdio\n");
  fprintf(stderr, "\t--file <file>\n");
  fprintf(stderr, "\t--tcp-l <port>\n");
  fprintf(stderr, "\t--tcp-c <addr>\n");
  fprintf(stderr, "\t--udp-l <port>\n");
  fprintf(stderr, "\t--udp-c <addr>\n");

  fprintf(stderr, "\nMisc options\n");
  fprintf(stderr, "\t--startup-delay <ms>\n");
  fprintf(stderr, "\t\ttime to delay after opening a socket\n");
  fprintf(stderr, "\t--nonblock\n");
  fprintf(stderr, "\t--debug\n");
  fprintf(stderr, "\t--outq <n>\n");
  fprintf(stderr, "\t\tmax tty output queue size (bytes)\n");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_STDIO = 1,
    OPT_ID_NAME,
    OPT_ID_FILE,
    OPT_ID_TCP_LISTEN,
    OPT_ID_TCP_CONNECT,
    OPT_ID_UDP_LISTEN,
    OPT_ID_UDP_CONNECT,
    OPT_ID_REP_TIMEOUT,
    OPT_ID_STARTUP_DELAY,
    OPT_ID_DEBUG,
    OPT_ID_FILTER_IN,
    OPT_ID_FILTER_OUT,
    OPT_ID_FILTER_IN_CONFIG,
    OPT_ID_FILTER_OUT_CONFIG,
    OPT_ID_NONBLOCK,
    OPT_ID_OUTQ,
  };

  const struct option long_opts[] = {
    {"pub",               required_argument, 0, 'p'},
    {"sub",               required_argument, 0, 's'},
    {"framer",            required_argument, 0, 'f'},
    {"stdio",             no_argument,       0, OPT_ID_STDIO},
    {"name",              required_argument, 0, OPT_ID_NAME},
    {"file",              required_argument, 0, OPT_ID_FILE},
    {"tcp-l",             required_argument, 0, OPT_ID_TCP_LISTEN},
    {"tcp-c",             required_argument, 0, OPT_ID_TCP_CONNECT},
    {"udp-l",             required_argument, 0, OPT_ID_UDP_LISTEN},
    {"udp-c",             required_argument, 0, OPT_ID_UDP_CONNECT},
    {"startup-delay",     required_argument, 0, OPT_ID_STARTUP_DELAY},
    {"filter-in",         required_argument, 0, OPT_ID_FILTER_IN},
    {"filter-out",        required_argument, 0, OPT_ID_FILTER_OUT},
    {"filter-in-config",  required_argument, 0, OPT_ID_FILTER_IN_CONFIG},
    {"filter-out-config", required_argument, 0, OPT_ID_FILTER_OUT_CONFIG},
    {"debug",             no_argument,       0, OPT_ID_DEBUG},
    {"nonblock",          no_argument,       0, OPT_ID_NONBLOCK},
    {"outq",              required_argument, 0, OPT_ID_OUTQ},
    {0, 0, 0, 0}
  };

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "p:s:r:y:f:",
                          long_opts, &opt_index)) != -1) {
    switch (c) {
      case OPT_ID_STDIO: {
        io_mode = IO_STDIO;
      }
      break;

      case OPT_ID_NAME: {
        port_name = optarg;
      }
      break;

      case OPT_ID_FILE: {
        io_mode = IO_FILE;
        char* rp = realpath(optarg, file_path);
        if (rp == NULL) {
          fprintf(stderr, "realpath returned error: %s\n", strerror(errno));
        }
        debug_printf("--file: %s (realpath: %s)\n", (char *)optarg, file_path);
      }
      break;

      case OPT_ID_TCP_LISTEN: {
        io_mode = IO_TCP_LISTEN;
        tcp_listen_port = strtol(optarg, NULL, 10);
      }
      break;

      case OPT_ID_TCP_CONNECT: {
        io_mode = IO_TCP_CONNECT;
        tcp_connect_addr = optarg;
      }
      break;

      case OPT_ID_UDP_LISTEN: {
        io_mode = IO_UDP_LISTEN;
        udp_listen_port = strtol(optarg, NULL, 10);
      }
      break;

      case OPT_ID_UDP_CONNECT: {
        io_mode = IO_UDP_CONNECT;
        udp_connect_addr = optarg;
      }
      break;

      case OPT_ID_STARTUP_DELAY: {
        startup_delay_ms = strtol(optarg, NULL, 10);
      }
      break;

      case OPT_ID_FILTER_IN: {
        if (filter_interface_valid(optarg) == 0) {
          filter_in_name = optarg;
        } else {
          fprintf(stderr, "invalid input filter\n");
          return -1;
        }
      }
      break;

      case OPT_ID_FILTER_OUT: {
        if (filter_interface_valid(optarg) == 0) {
          filter_out_name = optarg;
        } else {
          fprintf(stderr, "invalid output filter\n");
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

      case OPT_ID_NONBLOCK: {
        nonblock = true;
      }
      break;

      case OPT_ID_OUTQ: {
        outq = strtol(optarg, NULL, 10);
      }
      break;

      case 'p': {
        endpoint_mode = ENDPOINT_PUBSUB;
        pub_addr = optarg;
      }
      break;

      case 's': {
        endpoint_mode = ENDPOINT_PUBSUB;
        sub_addr = optarg;
      }
      break;

      case 'f': {
        if (framer_interface_valid(optarg) == 0) {
          framer_name = optarg;
        } else {
          fprintf(stderr, "invalid framer\n");
          return -1;
        }
      }
      break;

      default: {
        fprintf(stderr, "invalid option\n");
        return -1;
      }
      break;
    }
  }

  if (io_mode == IO_INVALID) {
    fprintf(stderr, "invalid mode\n");
    return -1;
  }

  if (endpoint_mode == ENDPOINT_INVALID) {
    fprintf(stderr, "endpoint address(es) not specified\n");
    return -1;
  }

  if ((strcasecmp(filter_in_name, FILTER_NONE_NAME) == 0) !=
      (filter_in_config == NULL)) {
    fprintf(stderr, "invalid input filter settings\n");
    return -1;
  }

  if ((strcasecmp(filter_out_name, FILTER_NONE_NAME) == 0) !=
      (filter_out_config == NULL)) {
    fprintf(stderr, "invalid output filter settings\n");
    return -1;
  }

  return 0;
}

static void terminate_handler(int signum)
{
  /* If this is the parent, send this signal to the entire process group */
  if (getpid() == getpgid(0)) {
    killpg(0, signum);
  }

  logging_deinit();

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

static int handle_init(handle_t *handle, pk_endpoint_t *pk_ept,
                       int read_fd, int write_fd,
                       const char *framer_name, const char *filter_name,
                       const char *filter_config)
{
  *handle = (handle_t) {
    .pk_ept = pk_ept,
    .read_fd = read_fd,
    .write_fd = write_fd,
    .framer = framer_create(framer_name),
    .filter = filter_create(filter_name, filter_config)
  };

  if ((handle->framer == NULL) || (handle->filter == NULL)) {
    handle_deinit(handle);
    return -1;
  }

  return 0;
}

static pk_endpoint_t * pk_endpoint_start(int type)
{
  const char *addr = NULL;
  switch (type) {
    case PK_ENDPOINT_PUB: {
      addr = pub_addr;
    }
    break;

    case PK_ENDPOINT_SUB: {
      addr = sub_addr;
    }
    break;

    default: {
      syslog(LOG_ERR, "unknown endpoint type");
    }
    break;
  }

  pk_endpoint_t *pk_ept = pk_endpoint_create(addr, type);
  if (pk_ept == NULL) {
    debug_printf("pk_endpoint_create returned NULL\n");
    return NULL;
  }

  usleep(1000 * startup_delay_ms);
  debug_printf("opened socket: %s\n", addr);
  return pk_ept;
}

static ssize_t write_with_count(pk_endpoint_t *pk_ept, const void *buffer, size_t count)
{
  if (pk_endpoint_send(pk_ept, (u8 *)buffer, count) != 0) {
    return -1;
  }
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

#define MSG_ERROR_SERIAL_FLUSH "Interface %s output buffer is full. Dropping data."

static ssize_t fd_write(int fd, const void *buffer, size_t count)
{
  if (isatty(fd) && (outq > 0)) {
    int qlen;
    ioctl(fd, TIOCOUTQ, &qlen);
    if (qlen + count > outq) {
      /* Flush the output buffer, otherwise we'll get behind and start
       * transmitting partial SBP packets, we must drop some data here, so we
       * choose to drop old data rather than new data.
       */
      tcflush(fd, TCOFLUSH);
      ioctl(fd, TIOCOUTQ, &qlen);
      if (qlen != 0) {
        if (strstr(port_name, "usb") != port_name) {
          piksi_log(LOG_WARNING, "Could not completely flush tty: %d bytes remaining.", qlen);
        } else {
          /* USB gadget serial can't flush properly for some reason, ignore...
           *   (This is ignored ad infinitum because this condition occurs on
           *   start-up before the interface is read from, after the interface
           *   is read from, it never occurs again.)
           */
          return count;
        }
      }
      piksi_log(LOG_ERR, MSG_ERROR_SERIAL_FLUSH, port_name);
      sbp_log(LOG_ERR, MSG_ERROR_SERIAL_FLUSH, port_name);
      return count;
    }
  }
  while (1) {
    ssize_t ret = write(fd, buffer, count);
    /* Retry if interrupted */
    if ((ret == -1) && (errno == EINTR)) {
      continue;
    } else if ((ret < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
      /* Our output buffer is full and we're in non-blocking mode.
       * Just silently drop the rest of the output...
       */
      return count;
    } else {
      return ret;
    }
  }
}

static ssize_t handle_read(handle_t *handle, void *buffer, size_t count)
{
  if (handle->pk_ept != NULL) {
    return pk_endpoint_read(handle->pk_ept, buffer, count);
  } else {
    return fd_read(handle->read_fd, buffer, count);
  }
}

static ssize_t handle_write(handle_t *handle, const void *buffer, size_t count)
{
  if (handle->pk_ept != NULL) {
    return write_with_count(handle->pk_ept, buffer, count);
  } else {
    return fd_write(handle->write_fd, buffer, count);
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
    if (write_count < 0) {
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
  while (1) {
    const uint8_t *frame;
    uint32_t frame_length;
    buffer_index +=
        framer_process(handle->framer,
                       &((uint8_t *)buffer)[buffer_index],
                       count - buffer_index,
                       &frame, &frame_length);
    if (frame == NULL) {
      return buffer_index;
    }

    /* Pass frame through filter */
    if (filter_process(handle->filter, frame, frame_length) != 0) {
      continue;
    }

    /* Write frame to handle */
    ssize_t write_count = handle_write_all(handle, frame, frame_length);
    if (write_count < 0) {
      return write_count;
    }
    if (write_count != frame_length) {
      syslog(LOG_ERR, "warning: write_count != frame_length");
    }

    *frames_written += 1;

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
  while (1) {
    size_t frames;
    ssize_t write_count =
        handle_write_one_via_framer(handle,
                                    &((uint8_t *)buffer)[buffer_index],
                                    count - buffer_index,
                                    &frames);
    if (write_count < 0) {
      return write_count;
    }

    buffer_index += write_count;

    if (frames == 0) {
      return buffer_index;
    }

    *frames_written += frames;
  }
  return buffer_index;
}

static void io_loop_pubsub(handle_t *read_handle, handle_t *write_handle)
{
  debug_printf("io loop begin\n");

  while (1) {

    PK_METRICS_UPDATE(MR, MI.read_count);

    /* Read from read_handle */
    uint8_t buffer[READ_BUFFER_SIZE];
    ssize_t read_count = handle_read(read_handle, buffer, sizeof(buffer));
    if (read_count <= 0) {
      debug_printf("read_count %d errno %s (%d)\n",
          read_count, strerror(errno), errno);
      break;
    }

    PK_METRICS_UPDATE(MR, MI.read_size_total, PK_METRICS_VALUE((u32) read_count));

    /* Write to write_handle via framer */
    size_t frames_written;
    ssize_t write_count = handle_write_all_via_framer(write_handle,
                                                      buffer, read_count,
                                                      &frames_written);
    if (write_count < 0) {
      debug_printf("write_count %d errno %s (%d)\n",
          write_count, strerror(errno), errno);
      break;
    }

    if (write_count != read_count) {
      syslog(LOG_ERR, "warning: write_count != read_count");
      debug_printf("write_count != read_count %d %d\n",
          write_count, read_count);
      PK_METRICS_UPDATE(MR, MI.mismatch);
    }

    do_metrics_flush();
  }

  debug_printf("io loop end\n");
}

static int pid_wait_check(pid_t *pid, pid_t wait_pid)
{
  if (*pid > 0) {
    if (*pid == wait_pid) {
      *pid = -1;
      return 0;
    }
  }

  return -1;
}

static void pid_terminate(pid_t *pid)
{
  if (*pid > 0) {
    if (kill(*pid, SIGTERM) != 0) {
      syslog(LOG_ERR, "error terminating pid %d", *pid);
    }
    *pid = -1;
  }
}

static void do_metrics_flush(void) 
{
  if (pk_metrics_gettime().ns - last_metrics_flush < one_second_ns) {
    return;
  }

  PK_METRICS_UPDATE(MR, MI.read_size_average);

  last_metrics_flush = pk_metrics_gettime().ns;

  pk_metrics_flush(MR);

  pk_metrics_reset(MR, MI.read_count);
  pk_metrics_reset(MR, MI.read_size_total);
  pk_metrics_reset(MR, MI.read_size_average);
} 

static void setup_metrics(const char* pubsub) {

  char suffix[128];
  size_t count = snprintf(suffix, sizeof(suffix), "%s_%s", port_name, pubsub);
  assert( count < sizeof(suffix) );

  MR = pk_metrics_setup("endpoint_adapter", suffix, MT, COUNT_OF(MT));

  if (MR == NULL) {
    syslog(LOG_ERR, "error configuring metrics");
    fprintf(stderr, "error configuring metrics\n");
    exit(EXIT_FAILURE);
  }

  last_metrics_flush = pk_metrics_gettime().ns;
}

void io_loop_start(int read_fd, int write_fd)
{
  if (nonblock && write_fd != -1) {
    int arg = O_NONBLOCK;
    fcntl(write_fd, F_SETFL, &arg);
  }
  switch (endpoint_mode) {
    case ENDPOINT_PUBSUB: {
      if (pub_addr != NULL && read_fd != -1) {
        debug_printf("Forking for pub\n");
        pid_t pid = fork();
        setup_metrics("pub");
        if (pid == 0) {
          /* child process */
          pk_endpoint_t *pub = pk_endpoint_start(PK_ENDPOINT_PUB);
          if (pub == NULL) {
            debug_printf("pk_endpoint_start(PK_ENDPOINT_PUB) returned NULL\n");
            exit(EXIT_FAILURE);
          }

          /* Read from fd, write to pub */
          handle_t pub_handle;
          if (handle_init(&pub_handle, pub, -1, -1, framer_name,
                          filter_in_name, filter_in_config) != 0) {
            debug_printf("handle_init for pub returned error\n");
            exit(EXIT_FAILURE);
          }

          handle_t fd_handle;
          if (handle_init(&fd_handle, NULL, read_fd, -1, FRAMER_NONE_NAME,
                          FILTER_NONE_NAME, NULL) != 0) {
            debug_printf("handle_init for read_fd returned error\n");
            exit(EXIT_FAILURE);
          }

          io_loop_pubsub(&fd_handle, &pub_handle);

          pk_endpoint_destroy(&pub);
          assert(pub == NULL);
          handle_deinit(&pub_handle);
          handle_deinit(&fd_handle);
          debug_printf("Exiting from pub fork\n");
          exit(EXIT_SUCCESS);
        } else {
          /* parent process */
          pub_pid = pid;
        }
      }

      if (sub_addr != NULL && write_fd != -1) {
        debug_printf("Forking for sub\n");
        setup_metrics("sub");
        pid_t pid = fork();
        if (pid == 0) {
          /* child process */
          pk_endpoint_t *sub = pk_endpoint_start(PK_ENDPOINT_SUB);
          if (sub == NULL) {
            debug_printf("pk_endpoint_start(PK_ENDPOINT_SUB) returned NULL\n");
            exit(EXIT_FAILURE);
          }

          /* Read from sub, write to fd */
          handle_t sub_handle;
          if (handle_init(&sub_handle, sub, -1, -1, FRAMER_NONE_NAME,
                          FILTER_NONE_NAME, NULL) != 0) {
            debug_printf("handle_init for sub returned error\n");
            exit(EXIT_FAILURE);
          }

          handle_t fd_handle;
          if (handle_init(&fd_handle, NULL, -1, write_fd, FRAMER_NONE_NAME,
                          filter_out_name, filter_out_config) != 0) {
            debug_printf("handle_init for write_fd returned error\n");
            exit(EXIT_FAILURE);
          }

          io_loop_pubsub(&sub_handle, &fd_handle);

          pk_endpoint_destroy(&sub);
          assert(sub == NULL);
          handle_deinit(&sub_handle);
          handle_deinit(&fd_handle);
          debug_printf("Exiting from sub fork\n");
          exit(EXIT_SUCCESS);
        } else {
          /* parent process */
          sub_pid = pid;
        }
      }

    }
    break;

    default:
      break;
  }
}

void io_loop_wait(void)
{
  while (1) {
    debug_printf("waiting for children state change\n");
    int ret = waitpid(-1, NULL, 0);
    debug_printf("waitpid returned %d errno %d\n", ret, errno);
    if ((ret == -1) && (errno == EINTR)) {
      /* Retry if interrupted */
      continue;
    } else {
      break;
      /* If any of the childs dies, then the parent process must exit
         and give the chance to the system to restart it */
    }
  }
  debug_printf("Exit from io_loop_wait\n");
}

/* Used in tcp_connect */
void io_loop_wait_one(void)
{
  while (1) {
    pid_t pid = waitpid(-1, NULL, 0);
    if ((pid == -1) && (errno == EINTR)) {
      /* Retry if interrupted */
      continue;
    } else if (pid >= 0) {
      int i;
      for (i = 0; i < sizeof(pids) / sizeof(pids[0]); i++) {
        if (pid_wait_check(&pub_pid, pid) == 0) {
          /* Return if a child from the list was terminated */
          return;
        }
      }
      /* Retry if the child was not in the list */
      continue;
    } else {
      /* Return on error */
      return;
    }
  }
}

void io_loop_terminate(void)
{
  int i;
  for (i = 0; i < sizeof(pids) / sizeof(pids[0]); i++) {
    pid_terminate(pids[i]);
  }
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  setpgid(0, 0); /* Set PGID = PID */

  const char *protocol_library_path = getenv(PROTOCOL_LIBRARY_PATH_ENV_NAME);
  if (protocol_library_path == NULL) {
    protocol_library_path = PROTOCOL_LIBRARY_PATH_DEFAULT;
  }

  if (protocols_import(protocol_library_path) != 0) {
    syslog(LOG_ERR, "error importing protocols");
    fprintf(stderr, "error importing protocols\n");
    exit(EXIT_FAILURE);
  }

  if (parse_options(argc, argv) != 0) {
    syslog(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  debug_printf("Parsed command line\n");

  signal(SIGPIPE, SIG_IGN); /* Allow write to return an error */
  signal(SIGIO, SIG_IGN);

  /* Set up handler for signals which should terminate the program */
  struct sigaction terminate_sa;
  terminate_sa.sa_handler = terminate_handler;
  sigemptyset(&terminate_sa.sa_mask);
  terminate_sa.sa_flags = 0;
  if ((sigaction(SIGINT, &terminate_sa, NULL) != 0) ||
      (sigaction(SIGTERM, &terminate_sa, NULL) != 0) ||
      (sigaction(SIGQUIT, &terminate_sa, NULL) != 0)) {
    syslog(LOG_ERR, "error setting up terminate handler");
    exit(EXIT_FAILURE);
  }

  int ret = 0;

  switch (io_mode) {
    case IO_STDIO: {
      extern int stdio_loop(void);
      ret = stdio_loop();
    }
    break;

    case IO_FILE: {
      extern int file_loop(const char *file_path, int need_read, int need_write);
      ret = file_loop(file_path, pub_addr ? 1 : 0, sub_addr ? 1 : 0);
    }
    break;

    case IO_TCP_LISTEN: {
      extern int tcp_listen_loop(int port);
      ret = tcp_listen_loop(tcp_listen_port);
    }
    break;

    case IO_TCP_CONNECT: {
      extern int tcp_connect_loop(const char *addr);
      ret = tcp_connect_loop(tcp_connect_addr);
    }
    break;

    case IO_UDP_LISTEN: {
      extern int udp_listen_loop(int port);
      ret = udp_listen_loop(udp_listen_port);
    }
    break;

    case IO_UDP_CONNECT: {
      extern int udp_connect_loop(const char *addr);
      ret = udp_connect_loop(udp_connect_addr);
    }
    break;

    default:
      break;
  }

  raise(SIGINT);
  return ret;
}
