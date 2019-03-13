/*
 * Copyright (C) 2016-2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#define _DEFAULT_SOURCE

#include <linux/can.h>
#include <stdlib.h>
#include <getopt.h>
#include <dlfcn.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>

#include <libpiksi/endpoint.h>
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
#define READ_BUFFER_SIZE (4 * 1024)
#define REP_TIMEOUT_DEFAULT_ms 10000
#define STARTUP_DELAY_DEFAULT_ms 0
#define ENDPOINT_RESTART_RETRY_COUNT 3
#define ENDPOINT_RESTART_RETRY_DELAY_ms 1
#define FRAMER_NONE_NAME "none"
#define FILTER_NONE_NAME "none"
#define METRIC_NAME_LEN 128

/* Sleep for a maximum 10ms while waiting for a send to complete */
#define MAX_SEND_SLEEP_MS (10)
#define SEND_SLEEP_NS (100)

#define PROGRAM_NAME "endpoint_adapter"

#define MI metrics_indexes
#define MT metrics_table
#define MR metrics_ref

static pk_metrics_t *MR = NULL;

/* clang-format off */
PK_METRICS_TABLE(MT, MI,

  PK_METRICS_ENTRY("error/mismatch",           "total",          M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, mismatch),
  PK_METRICS_ENTRY("error/send/bytes_dropped", "per_second",     M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, bytes_dropped),

  PK_METRICS_ENTRY("rx/frame/count",           "per_second",     M_U32,         M_UPDATE_SUM,     M_RESET_DEF, rx_frames),
  PK_METRICS_ENTRY("tx/frame/count",           "per_second",     M_U32,         M_UPDATE_SUM,     M_RESET_DEF, tx_frames),

  PK_METRICS_ENTRY("rx/read/count",            "per_second",     M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, rx_read_count),
  PK_METRICS_ENTRY("rx/read/size/per_second",  "total",          M_U32,         M_UPDATE_SUM,     M_RESET_DEF, rx_read_size_total),
  PK_METRICS_ENTRY("rx/read/size/per_second",  "average",        M_U32,         M_UPDATE_AVERAGE, M_RESET_DEF, rx_read_size_average,
                   M_AVERAGE_OF(MI,         rx_read_size_total,  rx_read_count)),

  PK_METRICS_ENTRY("tx/read/count",            "per_second",     M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, tx_read_count),
  PK_METRICS_ENTRY("tx/read/size/per_second",  "total",          M_U32,         M_UPDATE_SUM,     M_RESET_DEF, tx_read_size_total),
  PK_METRICS_ENTRY("tx/read/size/per_second",  "average",        M_U32,         M_UPDATE_AVERAGE, M_RESET_DEF, tx_read_size_average,
                   M_AVERAGE_OF(MI,         tx_read_size_total,  tx_read_count)),

  PK_METRICS_ENTRY("rx/write/count",           "per_second",     M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, rx_write_count),
  PK_METRICS_ENTRY("rx/write/size/per_second", "total",          M_U32,         M_UPDATE_SUM,     M_RESET_DEF, rx_write_size_total),
  PK_METRICS_ENTRY("rx/write/size/per_second", "average",        M_U32,         M_UPDATE_AVERAGE, M_RESET_DEF, rx_write_size_average,
                   M_AVERAGE_OF(MI,         rx_write_size_total, rx_write_count)),

  PK_METRICS_ENTRY("tx/write/count",           "per_second",     M_U32,         M_UPDATE_COUNT,   M_RESET_DEF, tx_write_count),
  PK_METRICS_ENTRY("tx/write/size/per_second", "total",          M_U32,         M_UPDATE_SUM,     M_RESET_DEF, tx_write_size_total),
  PK_METRICS_ENTRY("tx/write/size/per_second", "average",        M_U32,         M_UPDATE_AVERAGE, M_RESET_DEF, tx_write_size_average,
                   M_AVERAGE_OF(MI,         tx_write_size_total, tx_write_count))
)
/* clang-format on */

/* If pk_ept is NULL on the read handle, then we're reading from an external
 * handle in order to publish to an internal pubsub socket. */
#define UPDATE_IO_LOOP_METRIC(TheHandle, RxMetricIndex, TxMetricIndex, ...) \
  {                                                                         \
    if (TheHandle->pk_ept == NULL) {                                        \
      PK_METRICS_UPDATE(MR, RxMetricIndex, ##__VA_ARGS__);                  \
    } else {                                                                \
      PK_METRICS_UPDATE(MR, TxMetricIndex, ##__VA_ARGS__);                  \
    }                                                                       \
  }

typedef enum {
  IO_INVALID,
  IO_STDIO,
  IO_FILE,
  IO_TCP_LISTEN,
  IO_TCP_CONNECT,
  IO_UDP_LISTEN,
  IO_UDP_CONNECT,
  IO_CAN,
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

static void timer_handler(pk_loop_t *loop, void *handle, int status, void *context);
static void setup_metrics();

static void die_error(const char *error);

typedef ssize_t (*read_fn_t)(handle_t *handle, void *buffer, size_t count);
typedef ssize_t (*write_fn_t)(handle_t *handle, const void *buffer, size_t count);

typedef ssize_t (*read_buf_cb_t)(handle_t *read_handle, handle_t *write_handle, uint8_t *, size_t);

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

typedef struct {
  size_t total;
  ssize_t status;
  handle_t *read_handle;
  handle_t *write_handle;
  read_buf_cb_t read_buf_cb;
} read_ctx_t;

/* CLI options related globals */
bool debug = false;
static io_mode_t io_mode = IO_INVALID;
static endpoint_mode_t endpoint_mode = ENDPOINT_INVALID;
static const char *framer_in_name = FRAMER_NONE_NAME;
static const char *framer_out_name = FRAMER_NONE_NAME;
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
static int can_id = -1;
static int can_filter = -1;
static bool retry_pubsub = false;

static uint8_t fd_read_buffer[READ_BUFFER_SIZE]; /** The read buffer */
static bool eagain_warned = false; /** used to rate limit the EGAIN warning to once per second */

static void usage(char *command)
{
  fprintf(stderr, "Usage: %s\n", command);

  fprintf(stderr, "\nGeneral\n");
  fprintf(stderr, "\t--name\n");
  fprintf(stderr, "\t\tthe name of this adapter (typically used for metrics and logging)\n");

  fprintf(stderr, "\nEndpoint Modes - select one or two (see notes)\n");
  fprintf(stderr, "\t-p, --pub <addr>\n");
  fprintf(stderr, "\t\tsink socket, may be combined with --sub\n");
  fprintf(stderr, "\t-s, --sub <addr>\n");
  fprintf(stderr, "\t\tsource socket, may be combined with --pub\n");

  fprintf(stderr, "\nFramer Mode - optional\n");
  fprintf(stderr, "\t --framer-in <input-framer>\n");
  fprintf(stderr, "\t --framer-out <output-framer>\n");

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
  fprintf(stderr, "\t--can <can_id>\n");
  fprintf(stderr, "\t--can-f <can_filter>\n");

  fprintf(stderr, "\nMisc options\n");
  fprintf(stderr, "\t--startup-delay <ms>\n");
  fprintf(stderr, "\t\ttime to delay after opening a socket\n");
  fprintf(stderr, "\t--nonblock\n");
  fprintf(stderr, "\t--debug\n");
  fprintf(stderr, "\t--outq <n>\n");
  fprintf(stderr, "\t\tmax tty output queue size (bytes)\n");
  fprintf(stderr, "\t--retry\n");
  fprintf(stderr, "\t\tretry pub/sub endpoint connections\n");
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
    OPT_ID_CAN,
    OPT_ID_CAN_FILTER,
    OPT_ID_RETRY_PUBSUB,
    OPT_ID_FRAMER_IN,
    OPT_ID_FRAMER_OUT,
  };

  /* clang-format off */
  const struct option long_opts[] = {
    {"pub",               required_argument, 0, 'p'},
    {"sub",               required_argument, 0, 's'},
    {"framer-in",         required_argument, 0, OPT_ID_FRAMER_IN},
    {"framer-out",        required_argument, 0, OPT_ID_FRAMER_OUT},
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
    {"can",               required_argument, 0, OPT_ID_CAN},
    {"can-f",             required_argument, 0, OPT_ID_CAN_FILTER},
    {"retry",             no_argument,       0, OPT_ID_RETRY_PUBSUB},
    {0, 0, 0, 0},
  };
  /* clang-format on */

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "p:s:", long_opts, &opt_index)) != -1) {
    switch (c) {
    case OPT_ID_CAN: {
      io_mode = IO_CAN;
      can_id = atoi(optarg);
    } break;

    case OPT_ID_CAN_FILTER: {
      can_filter = atoi(optarg);
    } break;

    case OPT_ID_STDIO: {
      io_mode = IO_STDIO;
    } break;

    case OPT_ID_NAME: {
      port_name = optarg;
    } break;

    case OPT_ID_FILE: {
      io_mode = IO_FILE;
      char *rp = realpath(optarg, file_path);
      if (rp == NULL) {
        fprintf(stderr, "realpath returned error: %s\n", strerror(errno));
      }
      debug_printf("--file: %s (realpath: %s)\n", (char *)optarg, file_path);
    } break;

    case OPT_ID_TCP_LISTEN: {
      io_mode = IO_TCP_LISTEN;
      tcp_listen_port = strtol(optarg, NULL, 10);
    } break;

    case OPT_ID_TCP_CONNECT: {
      io_mode = IO_TCP_CONNECT;
      tcp_connect_addr = optarg;
    } break;

    case OPT_ID_UDP_LISTEN: {
      io_mode = IO_UDP_LISTEN;
      udp_listen_port = strtol(optarg, NULL, 10);
    } break;

    case OPT_ID_UDP_CONNECT: {
      io_mode = IO_UDP_CONNECT;
      udp_connect_addr = optarg;
    } break;

    case OPT_ID_STARTUP_DELAY: {
      startup_delay_ms = strtol(optarg, NULL, 10);
    } break;

    case OPT_ID_FILTER_IN: {
      if (filter_interface_valid(optarg) == 0) {
        filter_in_name = optarg;
      } else {
        fprintf(stderr, "invalid input filter\n");
        return -1;
      }
    } break;

    case OPT_ID_FILTER_OUT: {
      if (filter_interface_valid(optarg) == 0) {
        filter_out_name = optarg;
      } else {
        fprintf(stderr, "invalid output filter\n");
        return -1;
      }
    } break;

    case OPT_ID_FILTER_IN_CONFIG: {
      filter_in_config = optarg;
    } break;

    case OPT_ID_FILTER_OUT_CONFIG: {
      filter_out_config = optarg;
    } break;

    case OPT_ID_DEBUG: {
      debug = true;
    } break;

    case OPT_ID_NONBLOCK: {
      nonblock = true;
    } break;

    case OPT_ID_OUTQ: {
      outq = strtol(optarg, NULL, 10);
    } break;

    case 'p': {
      endpoint_mode = ENDPOINT_PUBSUB;
      pub_addr = optarg;
    } break;

    case 's': {
      endpoint_mode = ENDPOINT_PUBSUB;
      sub_addr = optarg;
    } break;

    case OPT_ID_FRAMER_IN: {
      if (framer_interface_valid(optarg) == 0) {
        framer_in_name = optarg;
      } else {
        fprintf(stderr, "invalid framer\n");
        return -1;
      }
    } break;

    case OPT_ID_FRAMER_OUT: {
      if (framer_interface_valid(optarg) == 0) {
        framer_out_name = optarg;
      } else {
        fprintf(stderr, "invalid framer\n");
        return -1;
      }
    } break;

    case OPT_ID_RETRY_PUBSUB: {
      retry_pubsub = true;
    } break;

    default: {
      fprintf(stderr, "invalid option\n");
      return -1;
    } break;
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

  if (port_name == NULL) {
    fprintf(stderr, "adapter name not set\n");
    return -1;
  }

  if ((strcasecmp(filter_in_name, FILTER_NONE_NAME) == 0) != (filter_in_config == NULL)) {
    fprintf(stderr, "invalid input filter settings\n");
    return -1;
  }

  if ((strcasecmp(filter_out_name, FILTER_NONE_NAME) == 0) != (filter_out_config == NULL)) {
    fprintf(stderr, "invalid output filter settings\n");
    return -1;
  }

  return 0;
}

static void terminate_handler(int signum)
{
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

  if (handle->pk_ept != NULL) {
    pk_endpoint_destroy(&handle->pk_ept);
    assert(handle->pk_ept == NULL);
  }

  if (handle->read_fd != -1) {
    close(handle->read_fd);
    handle->read_fd = -1;
  }

  if (handle->write_fd != -1) {
    close(handle->write_fd);
    handle->write_fd = -1;
  }
}

static int handle_init(handle_t *handle,
                       pk_endpoint_t *pk_ept,
                       int read_fd,
                       int write_fd,
                       const char *framer_name,
                       const char *filter_name,
                       const char *filter_config)
{
  *handle = (handle_t){
    .pk_ept = pk_ept,
    .read_fd = read_fd,
    .write_fd = write_fd,
    .framer = framer_create(framer_name),
    .filter = filter_create(filter_name, filter_config),
  };

  if ((handle->framer == NULL) || (handle->filter == NULL)) {
    handle_deinit(handle);
    return -1;
  }

  return 0;
}

static pk_endpoint_t *pk_endpoint_start(int type)
{
  char metric_name[METRIC_NAME_LEN] = {0};
  const char *addr = NULL;

  switch (type) {
  case PK_ENDPOINT_PUB: {
    addr = pub_addr;
    snprintf_assert(metric_name, sizeof(metric_name), "adapter/%s/pub", port_name);
  } break;

  case PK_ENDPOINT_SUB: {
    addr = sub_addr;
    snprintf_assert(metric_name, sizeof(metric_name), "adapter/%s/sub", port_name);
  } break;

  default: {
    PK_LOG_ANNO(LOG_ERR | LOG_SBP, "unknown endpoint type");
  } break;
  }

  pk_endpoint_t *pk_ept = pk_endpoint_create(pk_endpoint_config()
                                               .endpoint(addr)
                                               .identity(metric_name)
                                               .type(type)
                                               .retry_connect(retry_pubsub)
                                               .get());
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

static bool needs_outq_check(int fd)
{
  return isatty(fd) && outq > 0;
}

static bool ensure_outq_space(int fd, size_t count)
{
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
        return false;
      }
    }
    piksi_log(LOG_ERR | LOG_SBP, "Interface %s output buffer is full. Dropping data.", port_name);
    return false;
  }
  return true;
}

static ssize_t fd_write_with_timeout(int handle,
                                     const void *buffer,
                                     size_t count,
                                     const size_t max_send_sleep_ms,
                                     const size_t per_retry_sleep_ns)
{
  assert(per_retry_sleep_ns != 0);
  const size_t max_send_sleep_count = MS_TO_NS(max_send_sleep_ms) / per_retry_sleep_ns;
  size_t sleep_count = 0;
  for (;;) {
    ssize_t ret = write(handle, buffer, count);
    /* Retry if interrupted */
    if ((ret == -1) && (errno == EINTR)) {
      continue;
    } else if ((ret < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
      if (++sleep_count >= max_send_sleep_count) {
        nanosleep_autoresume(0, per_retry_sleep_ns);
        continue;
      }
      if (!eagain_warned) {
        PK_LOG_ANNO(LOG_WARNING,
                    "call to write() to send data returned EAGAIN for more than %d ms, "
                    "dropping %u queued bytes (endpoint ident: %s)",
                    max_send_sleep_ms,
                    count,
                    port_name);
        eagain_warned = true;
      }
      PK_METRICS_UPDATE(MR, MI.bytes_dropped, PK_METRICS_VALUE((u32)count));
      return count;
    } else {
      return ret;
    }
  }
}

static ssize_t fd_write(int handle, const void *buffer, size_t count)
{
  if (needs_outq_check(handle)) {
    if (!ensure_outq_space(handle, count)) {
      /* If `ensure_outq_space` fails, we're attempting to drop and flush data,
       * logging for this happens in `ensure_outq_space`, we need to fake
       * that we sent the data so an error won't be reported upstream.
       */
      return count;
    }
  }
  return fd_write_with_timeout(handle, buffer, count, MAX_SEND_SLEEP_MS, SEND_SLEEP_NS);
}

static ssize_t handle_write_all_via_framer(handle_t *handle, const uint8_t *buffer, size_t count);

static ssize_t process_read_buffer(handle_t *read_handle,
                                   handle_t *write_handle,
                                   uint8_t *buffer,
                                   size_t length)
{
  UPDATE_IO_LOOP_METRIC(read_handle, MI.rx_read_count, MI.tx_read_count);

  ssize_t write_count = handle_write_all_via_framer(write_handle, buffer, length);

  if (write_count < 0) {
    debug_printf("write_count %d errno %s (%d)\n", write_count, strerror(errno), errno);
    return -1;
  }

  if (write_count != length) {
    PK_LOG_ANNO(LOG_ERR, "input vs output mismatch: data read (%d) != data written(%d)");
    PK_METRICS_UPDATE(MR, MI.mismatch);
  }

  return length;
}

static int sub_ept_read(const uint8_t *buff, size_t length, void *context)
{
  read_ctx_t *read_ctx = (read_ctx_t *)context;

  if (read_ctx->read_buf_cb(read_ctx->read_handle, read_ctx->write_handle, (uint8_t *)buff, length)
      < 0) {
    read_ctx->status = -1;
  } else {
    read_ctx->total += length;
  }

  return read_ctx->status;
}

static ssize_t handle_write(handle_t *handle, const uint8_t *buffer, size_t count)
{
  if (handle->pk_ept != NULL) {

    PK_METRICS_UPDATE(MR, MI.rx_write_count);
    PK_METRICS_UPDATE(MR, MI.rx_write_size_total, PK_METRICS_VALUE((u32)count));

    if (pk_endpoint_send(handle->pk_ept, buffer, count) != 0) {
      return -1;
    }

    return count;
  }

  PK_METRICS_UPDATE(MR, MI.tx_write_count);
  PK_METRICS_UPDATE(MR, MI.tx_write_size_total, PK_METRICS_VALUE((u32)count));

  return fd_write(handle->write_fd, buffer, count);
}

static ssize_t handle_write_all(handle_t *handle, const uint8_t *buffer, size_t count)
{
  uint32_t buffer_index = 0;
  while (buffer_index < count) {
    ssize_t write_count = handle_write(handle, &buffer[buffer_index], count - buffer_index);
    if (write_count < 0) {
      return write_count;
    }
    buffer_index += write_count;
  }
  return buffer_index;
}

static ssize_t handle_write_one_via_framer(handle_t *handle,
                                           const uint8_t *buffer,
                                           size_t count,
                                           size_t *frames)
{
  uint32_t buffer_index = 0;
  for (;;) {
    const uint8_t *frame;
    uint32_t frame_length;
    uint32_t remaining = count - buffer_index;
    buffer_index +=
      framer_process(handle->framer, &buffer[buffer_index], remaining, &frame, &frame_length);
    if (frame == NULL) {
      break;
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
    *frames += 1;
    break;
  }
  return buffer_index;
}

static ssize_t handle_write_all_via_framer(handle_t *handle, const uint8_t *buffer, size_t bufsize)
{
  ssize_t write_result = -1;
  uint32_t buffer_index = 0;
  size_t frame_count = 0;
  for (;;) {
    size_t remaining = bufsize - buffer_index;
    write_result =
      handle_write_one_via_framer(handle, &buffer[buffer_index], remaining, &frame_count);
    if (write_result < 0) {
      break;
    }
    buffer_index += write_result;
    if (buffer_index >= bufsize) {
      write_result = bufsize;
      break;
    }
  }
  UPDATE_IO_LOOP_METRIC(handle, MI.tx_frames, MI.rx_frames, PK_METRICS_VALUE((u32)frame_count));
  return write_result;
}

static void io_loop_pubsub(pk_loop_t *loop, handle_t *read_handle, handle_t *write_handle)
{
  ssize_t rc = 0;

  if (read_handle->pk_ept != NULL) {
    read_ctx_t read_ctx = {
      .total = 0,
      .status = 0,
      .read_handle = read_handle,
      .write_handle = write_handle,
      .read_buf_cb = process_read_buffer,
    };
    rc = pk_endpoint_receive(loop_ctx.sub_ept, sub_ept_read, &read_ctx);
    if (rc != 0) {
      PK_LOG_ANNO(LOG_WARNING, "pk_endpoint_receive returned error: %d", rc);
    } else if (read_ctx.status == 0) {
      rc = read_ctx.total;
    } else {
      rc = read_ctx.status;
    }
  } else {
    rc = fd_read(read_handle->read_fd, fd_read_buffer, sizeof(fd_read_buffer));
    if (rc > 0) {
      rc = process_read_buffer(read_handle, write_handle, fd_read_buffer, rc);
    }
  }

  if (rc <= 0) {
    debug_printf("read returned code: %d\n", rc);
    pk_loop_stop(loop);
    return;
  }

  UPDATE_IO_LOOP_METRIC(read_handle,
                        MI.rx_read_size_total,
                        MI.tx_read_size_total,
                        PK_METRICS_VALUE((u32)rc));
}

static void timer_handler(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)handle;
  (void)status;
  (void)context;

  PK_METRICS_UPDATE(MR, MI.rx_read_size_average);
  PK_METRICS_UPDATE(MR, MI.rx_write_size_average);

  PK_METRICS_UPDATE(MR, MI.tx_read_size_average);
  PK_METRICS_UPDATE(MR, MI.tx_write_size_average);

  pk_metrics_flush(MR);

  pk_metrics_reset(MR, MI.bytes_dropped);

  pk_metrics_reset(MR, MI.rx_frames);
  pk_metrics_reset(MR, MI.tx_frames);

  pk_metrics_reset(MR, MI.rx_read_count);
  pk_metrics_reset(MR, MI.rx_read_size_total);
  pk_metrics_reset(MR, MI.rx_read_size_average);
  pk_metrics_reset(MR, MI.rx_write_count);
  pk_metrics_reset(MR, MI.rx_write_size_total);
  pk_metrics_reset(MR, MI.rx_write_size_average);

  pk_metrics_reset(MR, MI.tx_read_count);
  pk_metrics_reset(MR, MI.tx_read_size_total);
  pk_metrics_reset(MR, MI.tx_read_size_average);
  pk_metrics_reset(MR, MI.tx_write_count);
  pk_metrics_reset(MR, MI.tx_write_size_total);
  pk_metrics_reset(MR, MI.tx_write_size_average);

  eagain_warned = false;
}

static void setup_metrics()
{

  assert(MR == NULL);

  MR = pk_metrics_setup("endpoint_adapter", port_name, MT, COUNT_OF(MT));

  if (MR == NULL) {
    die_error("error configuring metrics");
  }
}

static void die_error(const char *error)
{
  piksi_log(LOG_ERR | LOG_SBP, error);
  fprintf(stderr, "%s\n", error);

  logging_deinit();

  exit(EXIT_FAILURE);
}

static bool handle_loop_status(pk_loop_t *loop, int status)
{
  if ((status & LOOP_DISCONNECTED) || (status & LOOP_ERROR)) {
    if (status & LOOP_ERROR) {
      /* Ignore EBADF since this happens when sockets or other
       * descriptors are closed and the loop kicks them out. */
      if (!pk_loop_match_last_error(loop, EBADF)) {
        PK_LOG_ANNO(LOG_WARNING,
                    "got error event callback from loop: %s, error: %s",
                    pk_loop_describe_status(status),
                    pk_loop_last_error(loop));
      }
    }
    pk_loop_stop(loop);
    return false;
  }
  if (status == LOOP_UNKNOWN) {
    PK_LOG_ANNO(LOG_WARNING, "loop status unknown");
    return false;
  }
  if (!(status & LOOP_READ)) {
    PK_LOG_ANNO(LOG_WARNING, "no loop read event");
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

int io_loop_run(int read_fd, int write_fd)
{
  loop_ctx.loop = pk_loop_create();
  setup_metrics();

  void *handle = pk_loop_timer_add(loop_ctx.loop, 1000, timer_handler, NULL);
  assert(handle != NULL);

  if (pub_addr != NULL && read_fd != -1) {

    loop_ctx.pub_ept = pk_endpoint_start(PK_ENDPOINT_PUB);
    if (loop_ctx.pub_ept == NULL) {
      die_error("pk_endpoint_start(PK_ENDPOINT_PUB) returned NULL\n");
    }

    loop_ctx.read_fd_handle = pk_loop_poll_add(loop_ctx.loop, read_fd, read_fd_cb, NULL);

    if (loop_ctx.read_fd_handle == NULL) {
      die_error("pk_loop_poll_add(...) returned NULL");
    }

    if (handle_init(&loop_ctx.pub_handle,
                    loop_ctx.pub_ept,
                    -1,
                    -1,
                    framer_in_name,
                    filter_in_name,
                    filter_in_config)
        != 0) {
      die_error("handle_init for pub returned error");
    }

    if (handle_init(&loop_ctx.read_handle,
                    NULL,
                    read_fd,
                    -1,
                    FRAMER_NONE_NAME,
                    FILTER_NONE_NAME,
                    NULL)
        != 0) {
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

    if (handle_init(&loop_ctx.sub_handle,
                    loop_ctx.sub_ept,
                    -1,
                    -1,
                    FRAMER_NONE_NAME,
                    FILTER_NONE_NAME,
                    NULL)
        != 0) {
      die_error("handle_init for sub returned error\n");
    }

    if (handle_init(&loop_ctx.write_handle,
                    NULL,
                    -1,
                    write_fd,
                    framer_out_name,
                    filter_out_name,
                    filter_out_config)
        != 0) {
      die_error("handle_init for write_fd returned error\n");
    }
  }

  int rc = pk_loop_run_simple(loop_ctx.loop);

  if (rc != 0) {
    PK_LOG_ANNO(LOG_WARNING, "pk_loop_run_simple returned error: %d", rc);
  }

  handle_deinit(&loop_ctx.pub_handle);
  handle_deinit(&loop_ctx.sub_handle);
  handle_deinit(&loop_ctx.read_handle);
  handle_deinit(&loop_ctx.write_handle);

  pk_loop_destroy(&loop_ctx.loop);
  pk_metrics_destroy(&MR);

  debug_printf("Exiting from io_loop_run ()\n");

  if (rc != 0) return IO_LOOP_ERROR;

  return IO_LOOP_SUCCESS;
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

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
  if ((sigaction(SIGINT, &terminate_sa, NULL) != 0)
      || (sigaction(SIGTERM, &terminate_sa, NULL) != 0)
      || (sigaction(SIGQUIT, &terminate_sa, NULL) != 0)) {
    syslog(LOG_ERR, "error setting up terminate handler");
    exit(EXIT_FAILURE);
  }

  int ret = 0;

  switch (io_mode) {
  case IO_STDIO: {
    extern int stdio_loop(void);
    ret = stdio_loop();
  } break;

  case IO_FILE: {
    extern int file_loop(const char *file_path, int need_read, int need_write);
    ret = file_loop(file_path, pub_addr ? 1 : 0, sub_addr ? 1 : 0);
  } break;

  case IO_TCP_LISTEN: {
    extern int tcp_listen_loop(int port);
    ret = tcp_listen_loop(tcp_listen_port);
  } break;

  case IO_TCP_CONNECT: {
    extern int tcp_connect_loop(const char *addr);
    ret = tcp_connect_loop(tcp_connect_addr);
  } break;

  case IO_UDP_LISTEN: {
    extern int udp_listen_loop(int port);
    ret = udp_listen_loop(udp_listen_port);
  } break;

  case IO_UDP_CONNECT: {
    extern int udp_connect_loop(const char *addr);
    ret = udp_connect_loop(udp_connect_addr);
  } break;

  case IO_CAN: {
    extern int can_loop(const char *name, u32 filter);
    ret = can_loop(port_name, can_filter);
  } break;

  default: break;
  }

  raise(SIGINT);
  return ret;
}
