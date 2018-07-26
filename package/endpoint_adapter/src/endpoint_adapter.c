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
#include <libpiksi/framer.h>
#include <libpiksi/filter.h>
#include <libpiksi/protocols.h>

#include "endpoint_adapter.h"

#define PROTOCOL_LIBRARY_PATH_ENV_NAME "PROTOCOL_LIBRARY_PATH"
#define PROTOCOL_LIBRARY_PATH_DEFAULT "/usr/lib/endpoint_protocols"
#define READ_BUFFER_SIZE (128*1024)
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

static pk_metrics_t* MR = NULL;

PK_METRICS_TABLE(MT, MI,

  PK_METRICS_ENTRY("error/mismatch",        "total",          M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, mismatch),

  PK_METRICS_ENTRY("ingress/read/count",            "per_second",     M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, ingress_read_count),
  PK_METRICS_ENTRY("ingress/read/size/per_second",  "total",          M_U32,         M_UPDATE_SUM,     M_RESET_DEF, ingress_read_size_total),
  PK_METRICS_ENTRY("ingress/read/size/per_second",  "average",        M_U32,         M_UPDATE_AVERAGE, M_RESET_DEF, ingress_read_size_average,
                   M_AVERAGE_OF(MI,         ingress_read_size_total,  ingress_read_count)),

  PK_METRICS_ENTRY("egress/read/count",            "per_second",     M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, egress_read_count),
  PK_METRICS_ENTRY("egress/read/size/per_second",  "total",          M_U32,         M_UPDATE_SUM,     M_RESET_DEF, egress_read_size_total),
  PK_METRICS_ENTRY("egress/read/size/per_second",  "average",        M_U32,         M_UPDATE_AVERAGE, M_RESET_DEF, egress_read_size_average,
                   M_AVERAGE_OF(MI,         egress_read_size_total,  egress_read_count)),

  PK_METRICS_ENTRY("ingress/write/count",           "per_second",     M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, ingress_write_count),
  PK_METRICS_ENTRY("ingress/write/size/per_second", "total",          M_U32,         M_UPDATE_SUM,     M_RESET_DEF, ingress_write_size_total),
  PK_METRICS_ENTRY("ingress/write/size/per_second", "average",        M_U32,         M_UPDATE_AVERAGE, M_RESET_DEF, ingress_write_size_average,
                   M_AVERAGE_OF(MI,         ingress_write_size_total, ingress_write_count)),

  PK_METRICS_ENTRY("egress/write/count",           "per_second",     M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, egress_write_count),
  PK_METRICS_ENTRY("egress/write/size/per_second", "total",          M_U32,         M_UPDATE_SUM,     M_RESET_DEF, egress_write_size_total),
  PK_METRICS_ENTRY("egress/write/size/per_second", "average",        M_U32,         M_UPDATE_AVERAGE, M_RESET_DEF, egress_write_size_average,
                   M_AVERAGE_OF(MI,         egress_write_size_total, egress_write_count))
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

static void do_metrics_flush(pk_loop_t *loop, void *handle, int status, void *context);
static void setup_metrics();

static void die_error(const char *error);

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
static const char *port_name = NULL;
static char file_path[PATH_MAX] = "";
static int tcp_listen_port = -1;
static const char *tcp_connect_addr = NULL;
static int udp_listen_port = -1;
static const char *udp_connect_addr = NULL;

static struct {
  pk_loop_t *loop;
  void *loop_sub_handle;
  void *read_fd_handle;
  pk_endpoint_t *pub_ept;
  pk_endpoint_t *sub_ept;
  handle_t pub_handle;
  handle_t sub_handle;
  handle_t read_handle;
  handle_t write_handle;
} loop_ctx;

static struct {
  u8* buffer;
  size_t fill;
  size_t size;
} read_ctx = {
  .buffer = NULL,
  .fill = 0,
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

  if (port_name == NULL) {
    fprintf(stderr, "a port name is required\n");
    return -1;
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

static int sub_ept_read(const u8 *buff, size_t length, void *context)
{
  if (length > read_ctx.size) {

    piksi_log(LOG_ERR|LOG_SBP, "%s: received more data we can ingest, dropping %zu bytes (%s:%d)",
              __FUNCTION__, (length - read_ctx.size), __FILE__, __LINE__);

    length = read_ctx.size;
  }

  memcpy(read_ctx.buffer, buff, length);
  read_ctx.fill += length;

  // Return -1 to terminate the read loop, only read one packet
  return -1;
}

static ssize_t handle_read(handle_t *handle, u8* buffer, size_t count)
{
  if (handle->pk_ept != NULL) {

    read_ctx.buffer = buffer;
    read_ctx.size = count;
    read_ctx.fill = 0;

    int rc = pk_endpoint_receive(loop_ctx.sub_ept, sub_ept_read, &read_ctx);

    if (rc != 0) {
      piksi_log(LOG_WARNING, "%s: pk_endpoint_receive returned error: %d (%s:%d)",
                __FUNCTION__, rc, __FILE__, __LINE__);
    }

    return read_ctx.fill;

  } else {
    return fd_read(handle->read_fd, buffer, count);
  }
}

static ssize_t handle_write(handle_t *handle, const void *buffer, size_t count)
{
  if (handle->pk_ept != NULL) {

    PK_METRICS_UPDATE(MR, MI.ingress_write_count);
    PK_METRICS_UPDATE(MR, MI.ingress_write_size_total, PK_METRICS_VALUE((u32) count));

    if (pk_endpoint_send(handle->pk_ept, (u8 *)buffer, count) != 0) {
      return -1;
    }

    return count;

  } else {

    PK_METRICS_UPDATE(MR, MI.egress_write_count);
    PK_METRICS_UPDATE(MR, MI.egress_write_size_total, PK_METRICS_VALUE((u32) count));

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

static void io_loop_pubsub(pk_loop_t* loop, handle_t *read_handle, handle_t *write_handle)
{
  if (read_handle->pk_ept != NULL) {
    PK_METRICS_UPDATE(MR, MI.egress_read_count);
  } else {
    PK_METRICS_UPDATE(MR, MI.ingress_read_count);
  }

  /* Read from read_handle */
  static uint8_t buffer[READ_BUFFER_SIZE];
  ssize_t read_count = handle_read(read_handle, buffer, sizeof(buffer));
  if (read_count <= 0) {
    debug_printf("read_count %d errno %s (%d)\n",
        read_count, strerror(errno), errno);
    pk_loop_stop(loop);
    return;
  }

  if (read_handle->pk_ept != NULL) {
    PK_METRICS_UPDATE(MR, MI.egress_read_size_total, PK_METRICS_VALUE((u32) read_count));
  } else {
    PK_METRICS_UPDATE(MR, MI.ingress_read_size_total, PK_METRICS_VALUE((u32) read_count));
  }

  /* Write to write_handle via framer */
  size_t frames_written;
  ssize_t write_count = handle_write_all_via_framer(write_handle,
                                                    buffer, read_count,
                                                    &frames_written);
  if (write_count < 0) {
    debug_printf("write_count %d errno %s (%d)\n",
        write_count, strerror(errno), errno);
    pk_loop_stop(loop);
    return;
  }

  if (write_count != read_count) {
    syslog(LOG_ERR, "warning: write_count != read_count");
    debug_printf("write_count != read_count %d %d\n",
        write_count, read_count);
    PK_METRICS_UPDATE(MR, MI.mismatch);
  }
}

static void do_metrics_flush(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void) loop;
  (void) handle;
  (void) status;
  (void) context;

  PK_METRICS_UPDATE(MR, MI.ingress_read_size_average);
  PK_METRICS_UPDATE(MR, MI.ingress_write_size_average);

  PK_METRICS_UPDATE(MR, MI.egress_read_size_average);
  PK_METRICS_UPDATE(MR, MI.egress_write_size_average);

  pk_metrics_flush(MR);

  pk_metrics_reset(MR, MI.ingress_read_count);
  pk_metrics_reset(MR, MI.ingress_read_size_total);
  pk_metrics_reset(MR, MI.ingress_read_size_average);
  pk_metrics_reset(MR, MI.ingress_write_count);
  pk_metrics_reset(MR, MI.ingress_write_size_total);
  pk_metrics_reset(MR, MI.ingress_write_size_average);

  pk_metrics_reset(MR, MI.egress_read_count);
  pk_metrics_reset(MR, MI.egress_read_size_total);
  pk_metrics_reset(MR, MI.egress_read_size_average);
  pk_metrics_reset(MR, MI.egress_write_count);
  pk_metrics_reset(MR, MI.egress_write_size_total);
  pk_metrics_reset(MR, MI.egress_write_size_average);
}

static void setup_metrics() {

  assert( MR == NULL );

  MR = pk_metrics_setup("endpoint_adapter", port_name, MT, COUNT_OF(MT));

  if (MR == NULL) {
    die_error("error configuring metrics");
    exit(EXIT_FAILURE);
  }
}

static void die_error(const char *error)
{
  piksi_log(LOG_ERR|LOG_SBP, error);
  fprintf(stderr, "%s\n", error);

  exit(EXIT_FAILURE);
}

static bool handle_loop_status(pk_loop_t *loop, int status)
{
  if ((status & LOOP_DISCONNECTED) || (status & LOOP_ERROR)) {
    if (status & LOOP_ERROR) {
      piksi_log(LOG_WARNING, "%s: got error event callback from loop: %s (%s:%d)",
                __FUNCTION__, pk_loop_describe_status(status), __FILE__, __LINE__);
    }
    pk_loop_stop(loop);
    return false;
  }
  return true;
}

static void sub_reader_cb(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)handle;
  (void)context;

  if (!handle_loop_status(loop, status)) return;

  io_loop_pubsub(loop_ctx.loop, &loop_ctx.sub_handle, &loop_ctx.write_handle);
}

static void read_fd_cb(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)handle;
  (void)context;

  if (!handle_loop_status(loop, status)) return;

  io_loop_pubsub(loop, &loop_ctx.read_handle, &loop_ctx.pub_handle);
}

int io_loop_start(int read_fd, int write_fd, bool fork_needed)
{
  if (write_fd != -1) {
    int arg = O_NONBLOCK;
    fcntl(write_fd, F_SETFL, &arg);
  }

  if (read_fd != -1) {
    int arg = O_NONBLOCK;
    fcntl(write_fd, F_SETFL, &arg);
  }

  if (fork_needed) {
    pid_t pid = fork();
    if (pid != 0) {
      return 0;
    }
  }

  loop_ctx.loop = pk_loop_create();

  setup_metrics();

  void *handle =
    pk_loop_timer_add(loop_ctx.loop,
                      1000,
                      do_metrics_flush,
                      NULL);

  assert( handle != NULL );


  if (pub_addr != NULL && read_fd != -1) {

    loop_ctx.pub_ept = pk_endpoint_start(PK_ENDPOINT_PUB);
    if (loop_ctx.pub_ept == NULL) {
      die_error("pk_endpoint_start(PK_ENDPOINT_PUB) returned NULL\n");
    }

    loop_ctx.read_fd_handle =
      pk_loop_poll_add(loop_ctx.loop, read_fd, read_fd_cb, NULL);

    if (handle_init(&loop_ctx.pub_handle, loop_ctx.pub_ept, -1, -1, framer_name,
                    filter_in_name, filter_in_config) != 0) {
      die_error("handle_init for pub returned error");
    }

    if (handle_init(&loop_ctx.read_handle, NULL, read_fd, -1, FRAMER_NONE_NAME,
                    FILTER_NONE_NAME, NULL) != 0) {
      die_error("handle_init for read_fd returned error\n");
    }
  }

  if (sub_addr != NULL && write_fd != -1) {

    loop_ctx.sub_ept = pk_endpoint_start(PK_ENDPOINT_SUB);
    if (loop_ctx.sub_ept == NULL) {
      die_error("pk_endpoint_start(PK_ENDPOINT_SUB) returned NULL");
    }

    loop_ctx.loop_sub_handle =
      pk_loop_endpoint_reader_add(loop_ctx.loop, loop_ctx.sub_ept, sub_reader_cb, NULL);

    if (handle_init(&loop_ctx.sub_handle, loop_ctx.sub_ept, -1, -1, FRAMER_NONE_NAME,
                    FILTER_NONE_NAME, NULL) != 0) {
      die_error("handle_init for sub returned error\n");
    }

    if (handle_init(&loop_ctx.write_handle, NULL, -1, write_fd, FRAMER_NONE_NAME,
                    filter_out_name, filter_out_config) != 0) {
      die_error("handle_init for write_fd returned error\n");
    }
  }

  int rc = pk_loop_run_simple(loop_ctx.loop);

  if (rc != 0) {
    piksi_log(LOG_WARNING, "%s: pk_loop_run_simple returned error: %d (%s:%d)",
              __FUNCTION__, rc, __FILE__, __LINE__);
  }

  handle_deinit(&loop_ctx.pub_handle);
  handle_deinit(&loop_ctx.sub_handle);
  handle_deinit(&loop_ctx.read_handle);
  handle_deinit(&loop_ctx.write_handle);

  pk_loop_destroy(&loop_ctx.loop);
  pk_metrics_destroy(&MR);

  debug_printf("Exiting from pubsub (fork: %s)\n", fork_needed ? "y" : "n");

  return -1;
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
