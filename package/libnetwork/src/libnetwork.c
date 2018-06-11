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

#define _GNU_SOURCE

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include <linux/sockios.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <curl/curl.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "libnetwork.h"

/** Maximum length of username string */
#define LIBNETWORK_USERNAME_MAX_LENGTH (512L)
/** Maximum length of password string */
#define LIBNETWORK_PASSWORD_MAX_LENGTH (512L)
/** Maximum length of url string */
#define LIBNETWORK_URL_MAX_LENGTH (4096L)

/** How large to configure the recv buffer to avoid excessive buffering. */
const long RECV_BUFFER_SIZE = 4096L;
/** Max number of callbacks from CURLOPT_XFERINFOFUNCTION before we attempt to
 * reconnect to the server */
const curl_off_t MAX_STALLED_INTERVALS = 300;

/** Threshold at which to issue a warning that the pipe will fill up. */
const double PIPE_WARN_THRESHOLD = 0.90;
/** Min amount time between "pipe full" warning messages. */
const double PIPE_WARN_SECS = 5;

/** How often CURL errors should be reported */
const time_t ERROR_REPORTING_INTERVAL = 10;

/** Max number of consecutive errors reading GGA sentence file */
const int MAX_GGA_UPLOAD_READ_ERRORS = 5;

typedef struct context_node context_node_t;

typedef struct {

  bool configured;

  int req_fd;
  int rep_fd;

  int req_read_fd;
  int rep_write_fd;

  char req_path[PATH_MAX];
  char rep_path[PATH_MAX];

} fifo_info_t;

struct network_context_s {

  network_type_t type;         /**< The type of the network session */

  int fd;                      /**< The input fd to read from */
  bool debug;                  /**< Set if we are emitted debug information */
  curl_socket_t socket_fd;     /**< The socket we're read/writing from/to */

  CURL *curl;                  /**< The current cURL handle */

  curl_off_t bytes_transfered; /**< Current number of bytes transferred */
  curl_off_t stall_count;      /**< Number of times the bytes transferred have not changed */

  long response_code;          /**< Response Code from last transfer request */
  int (*response_code_check)(network_context_t* ctx); /**< Optional function reference to act on response code*/

  volatile bool shutdown_signaled;
  volatile bool cycle_connection_signaled;

  context_node_t* node;

  bool report_errors;           /**< Should this instance of libnetwork report errors? */
  time_t last_curl_error_time;  /**< Time at which we last issued a cURL error message */

  char username[LIBNETWORK_USERNAME_MAX_LENGTH];
  char password[LIBNETWORK_PASSWORD_MAX_LENGTH];
  char url[LIBNETWORK_URL_MAX_LENGTH];

  fifo_info_t control_fifo_info;

  double gga_xfer_secs;        /**< The number of seconds between GGA upload times */
  time_t last_xfer_time;       /**< Time of the last GGA sentence uploaded (for NTRIP only) */

  char* gga_xfer_buffer;       /**< Buffer to cache the last GGA sentence uploaded */
  size_t gga_xfer_buflen;      /**< The max capacity of the GGA sentence cache */
  size_t gga_xfer_fill;        /**< How much of the GGA sentence buffer is filled */

  int gga_error_count;         /**< Number of consecutive errors reading GGA file */

  bool gga_rev1;               /**< Should we use rev1 style GGA sentence? */
};

struct context_node {
  network_context_t context;
  LIST_ENTRY(context_node) entries;
};

typedef LIST_HEAD(context_nodes_head, context_node) context_nodes_head_t;

context_nodes_head_t context_nodes_head = LIST_HEAD_INITIALIZER(context_nodes_head);

static network_context_t empty_context = {
  .type = NETWORK_TYPE_INVALID,
  .fd = -1,
  .debug = false,
  .socket_fd = CURL_SOCKET_BAD,
  .curl = NULL,
  .bytes_transfered = 0,
  .stall_count = 0,
  .shutdown_signaled = false,
  .cycle_connection_signaled = false,
  .response_code = 0,
  .response_code_check = NULL,
  .node = NULL,
  .report_errors = true,
  .last_curl_error_time = 0,
  .username = "",
  .password = "",
  .url = "",
  .control_fifo_info = {
    .configured = false,
    .req_fd = -1,
    .rep_fd = -1,
    .req_read_fd = -1,
    .rep_write_fd = -1,
    .req_path = "",
    .rep_path = "",
  },
  .gga_xfer_secs = -1.0,
  .last_xfer_time = 0,
  .gga_xfer_buffer = NULL,
  .gga_xfer_buflen = 0,
  .gga_xfer_fill = 0,
  .gga_error_count = 0,
  .gga_rev1 = false,
};

#define NMEA_GGA_FILE "/var/run/nmea/GGA"

#define HTTP_RESPONSE_CODE_OK (200L)
#define NTRIP_DROPPED_CONNECTION_WARNING "Connection dropped with no data. This may be because this NTRIP caster expects an NMEA GGA string to be sent from the receiver. You can enable this through the ntrip.gga_period setting."

static void trim_crlf(char* buf, size_t *byte_count) __attribute__((nonnull(1)));
static void log_with_rate_limit(network_context_t* ctx, int priority, const char *format, ...)
  __attribute__((nonnull(1,3)));

void libnetwork_shutdown()
{
  context_node_t* node;
  LIST_FOREACH(node, &context_nodes_head, entries) {
    node->context.shutdown_signaled = true;
  }
}

void libnetwork_cycle_connection()
{
  context_node_t* node;
  LIST_FOREACH(node, &context_nodes_head, entries) {
    node->context.cycle_connection_signaled = true;
  }
}

void libnetwork_report_errors(network_context_t *ctx, bool yesno)
{
  ctx->report_errors = yesno;
}

network_status_t libnetwork_configure_control(network_context_t *ctx, control_pair_t control_pair)
{
  unlink(control_pair.req_fifo_name);
  unlink(control_pair.rep_fifo_name);

  mode_t umask_previous = umask(0);
  int req_fd = mkfifo(control_pair.req_fifo_name, 0777);
  if (req_fd < 0) {
    piksi_log(LOG_ERR, "error opening request FIFO (%s) (error: %d) \"%s\"", control_pair.req_fifo_name, errno, strerror(errno));
    umask(umask_previous);
    return NETWORK_STATUS_FIFO_ERROR;
  }

  int rep_fd = mkfifo(control_pair.rep_fifo_name, 0777);
  if (rep_fd < 0) {
    piksi_log(LOG_ERR, "error opening response FIFO (%s) (error: %d) \"%s\"", control_pair.rep_fifo_name, errno, strerror(errno));
    close(req_fd);
    umask(umask_previous);
    return NETWORK_STATUS_FIFO_ERROR;
  }

  umask(umask_previous);

  ctx->control_fifo_info.configured = true;

  ctx->control_fifo_info.req_fd = req_fd;
  ctx->control_fifo_info.rep_fd = rep_fd;

  strncpy(ctx->control_fifo_info.req_path, control_pair.req_fifo_name, sizeof(ctx->control_fifo_info.req_path));
  strncpy(ctx->control_fifo_info.rep_path, control_pair.rep_fifo_name, sizeof(ctx->control_fifo_info.rep_path));

  return NETWORK_STATUS_SUCCESS;
}

network_status_t libnetwork_request_health(control_pair_t control_pair, int *status)
{
  int req_fd = -1;
  int rep_fd = -1;

  network_status_t return_status = NETWORK_STATUS_SUCCESS;

  req_fd = open(control_pair.req_fifo_name, O_WRONLY);
  if (req_fd < 0) {
    piksi_log(LOG_ERR, "request fifo error (%d) \"%s\"", errno, strerror(errno));
    return_status = NETWORK_STATUS_FIFO_ERROR;
    goto libnetwork_request_health_exit;
  }

  int wc = write(req_fd, CONTROL_COMMAND_STATUS, 1);
  if (wc <= 0) {
    return_status = NETWORK_STATUS_WRITE_ERROR;
    goto libnetwork_request_health_exit;
  }

  close(req_fd);

  rep_fd = open(control_pair.rep_fifo_name, O_RDONLY);
  if (rep_fd < 0) {
    piksi_log(LOG_ERR, "response fifo error (%d) \"%s\"", errno, strerror(errno));
    return_status = NETWORK_STATUS_FIFO_ERROR;
    goto libnetwork_request_health_exit;
  }

  char response_buf[4] = { 0 };
  int rc = read(rep_fd, response_buf, sizeof(response_buf) - 1);
  if (rc <= 0) {
    return_status = NETWORK_STATUS_READ_ERROR;
    goto libnetwork_request_health_exit;
  }

  close(rep_fd);

  long response = strtol(response_buf, NULL, 10);
  if (response < 0) {
    piksi_log(LOG_WARNING, "%s: error requesting skylark HTTP response code: %d", __FUNCTION__, response);
  }

  if (status != NULL) {
    *status = (int) response;
  }

 libnetwork_request_health_exit:
  close(rep_fd);
  close(req_fd);

  return return_status;
}

network_context_t* libnetwork_create(network_type_t type)
{
  context_node_t* node = malloc(sizeof(context_node_t));
  if (node == NULL)
    return NULL;

  memcpy(&node->context, &empty_context, sizeof(empty_context));

  node->context.type = type;
  node->context.node = node;

  LIST_INSERT_HEAD(&context_nodes_head, node, entries);

  return &node->context;
}

void libnetwork_destroy(network_context_t **ctx)
{
  LIST_REMOVE((*ctx)->node, entries);

  int* fd_list[] = {
    &(*ctx)->control_fifo_info.req_fd,
    &(*ctx)->control_fifo_info.req_read_fd,
    &(*ctx)->control_fifo_info.rep_fd,
    &(*ctx)->control_fifo_info.rep_write_fd,
  };

  for (size_t x = 0; x < COUNT_OF(fd_list); x++) {
    if (*fd_list[x] < 0)
      continue;
    close(*fd_list[x]);
    *fd_list[x] = -1;
  }

  if ((*ctx)->gga_xfer_buffer != NULL)
    free((*ctx)->gga_xfer_buffer);

  free(*ctx);
  *ctx = NULL;
}

network_status_t libnetwork_set_fd(network_context_t* ctx, int fd)
{
  ctx->fd = fd;
  return NETWORK_STATUS_SUCCESS;
}

network_status_t libnetwork_set_username(network_context_t* context, const char* username)
{
  const size_t max_username = sizeof(((network_context_t*)NULL)->username);

  if (strlen(username) + 1 > max_username)
    return NETWORK_STATUS_URL_TOO_LARGE;

  (void)strncpy(context->username, username, max_username-1);

  return NETWORK_STATUS_SUCCESS;
}

network_status_t libnetwork_set_password(network_context_t* context, const char* password)
{
  const size_t max_password = sizeof(((network_context_t*)NULL)->password);

  if (strlen(password) + 1 > max_password)
    return NETWORK_STATUS_URL_TOO_LARGE;

  (void)strncpy(context->password, password, max_password-1);

  return NETWORK_STATUS_SUCCESS;
}

network_status_t libnetwork_set_url(network_context_t* context, const char* url)
{
  const size_t max_url = sizeof(((network_context_t*)NULL)->url);

  if (strlen(url) + 1 > max_url)
    return NETWORK_STATUS_URL_TOO_LARGE;

  (void)strncpy(context->url, url, max_url-1);

  return NETWORK_STATUS_SUCCESS;
}

network_status_t libnetwork_set_debug(network_context_t* context, bool debug)
{
  context->debug = debug;
  return NETWORK_STATUS_SUCCESS;
}

network_status_t libnetwork_set_gga_upload_interval(network_context_t* context, int gga_interval)
{
  if (context->type != NETWORK_TYPE_NTRIP_DOWNLOAD) {
    return NETWORK_STATUS_INVALID_SETTING;
  }

  context->gga_xfer_secs = gga_interval;

  return NETWORK_STATUS_SUCCESS;
}

network_status_t libnetwork_set_gga_upload_rev1(network_context_t* context, bool use_rev1)
{
  context->gga_rev1 = use_rev1;
  return NETWORK_STATUS_SUCCESS;
}

static void warn_on_pipe_full(int fd, size_t pending_write, bool debug)
{
  static time_t last_pipe_warn_time = 0;

  int outq_size = 0;

  if (ioctl(fd, FIONREAD, &outq_size) < 0) {
    piksi_log(LOG_ERR, "ioctl error %d", errno);
    return;
  }

  int pipe_size = fcntl(fd, F_GETPIPE_SZ);

  if (pipe_size < 0) {
    piksi_log(LOG_ERR, "fcntl error %d (pipe_size=%d)", errno, pipe_size);
    return;
  }

  if (debug) {
    piksi_log(LOG_DEBUG, "fifo has %d bytes pending read, %d total bytes", outq_size, pipe_size);
  }

  time_t now = time(NULL);
  double percent_full = ((outq_size + (int)pending_write) / (double)pipe_size);

  if ( percent_full >= PIPE_WARN_THRESHOLD && now - last_pipe_warn_time >= PIPE_WARN_SECS) {
    const char* msg = "output fifo almost full, future writes will block";
    sbp_log(LOG_WARNING, msg);
    piksi_log(LOG_WARNING, msg);
    last_pipe_warn_time = now;
  }
}

static void dump_connection_stats(int fd)
{
  struct tcp_info tcp_info;
  socklen_t tcp_info_len = sizeof(tcp_info);

  if (getsockopt(fd, SOL_TCP, TCP_INFO, (void*)&tcp_info, &tcp_info_len) < 0) {
    piksi_log(LOG_ERR, "getsockopt error %d", errno);

  } else {
    piksi_log(LOG_DEBUG, "rtt=%u rttvar=%u rcv_rtt=%u rcv_space=%u rcv_mss=%u advmss=%u\n",
        tcp_info.tcpi_rtt,
        tcp_info.tcpi_rttvar,
        tcp_info.tcpi_rcv_rtt,
        tcp_info.tcpi_rcv_space,
        tcp_info.tcpi_rcv_mss,
        tcp_info.tcpi_advmss);
  }
}

static size_t network_download_write(char *buf, size_t size, size_t n, void *data)
{
  network_context_t *ctx = data;

  if (ctx->debug) {
    dump_connection_stats(ctx->socket_fd);
  }

  warn_on_pipe_full(ctx->fd, size * n, ctx->debug);

  while (true) {

    ssize_t ret = write(ctx->fd, buf, size * n);
    if (ret < 0 && errno == EINTR) {
      continue;
    }

    if (ctx->debug) {
      piksi_log(LOG_DEBUG, "write bytes (%d) %d", size * n, ret);
    }

    return ret;
  }

  return -1;
}

static size_t network_upload_read(char *buf, size_t size, size_t n, void *data)
{
  network_context_t *ctx = data;

  while (true) {
    ssize_t ret = read(ctx->fd, buf, size * n);
    if (ret < 0 && errno == EINTR) {
      continue;
    }

    if (ctx->debug) {
      piksi_log(LOG_DEBUG, "read bytes %d", ret);
    }

    if (ret < 0) {
      return CURL_READFUNC_ABORT;
    }

    return ret;
  }

  return -1;
}

static void trim_crlf(char *buf, size_t *len)
{
  char *crlf = strstr(buf, "\r\n");
  if (crlf != NULL) *crlf = '\0';
  if (len != NULL) *len = strlen(buf);
}

static void cache_gga_xfer_buffer(network_context_t *ctx, char* buf, size_t buflen, size_t fill)
{
  if (ctx->gga_xfer_buffer == NULL || buflen != ctx->gga_xfer_buflen) {
    if (ctx->gga_xfer_buffer != NULL) free(ctx->gga_xfer_buffer);
    ctx->gga_xfer_buffer = malloc(buflen);
    ctx->gga_xfer_buflen = buflen;
  }

  memcpy(ctx->gga_xfer_buffer, buf, fill);
  ctx->gga_xfer_fill = fill;
}

static size_t fill_with_gga_xfer_cache(network_context_t *ctx, char* buf, size_t buflen)
{
  // If there's no cache, pause
  if (ctx->gga_xfer_buffer == NULL) {
    piksi_log(LOG_DEBUG, "%s: no cached GGA sentence is present", __FUNCTION__);
    return 0;
  }

  size_t fill = ctx->gga_xfer_fill;

  if (ctx->gga_xfer_fill > buflen) {
    piksi_log(LOG_WARNING, "%s: cached GGA sentence is larger than provided buffer", __FUNCTION__);
    fill = buflen;
  }

  memcpy(buf, ctx->gga_xfer_buffer, fill);

  return fill;
}

static size_t fetch_gga_buffer(network_context_t *ctx, char *buf, size_t buf_size)
{
  FILE *fp_gga_cache = fopen(NMEA_GGA_FILE, "r");

  if (fp_gga_cache == NULL) {
    piksi_log(LOG_WARNING, "failed to open '" NMEA_GGA_FILE "' file: %s", strerror(errno));
    return fill_with_gga_xfer_cache(ctx, buf, buf_size);
  }

  if (ctx->debug) {
    piksi_log(LOG_DEBUG, "provided buffer size: %lu", buf_size);
  }

  // Subtract one to ensure we can null terminate
  size_t read_count = fread(buf, 1, buf_size - 1, fp_gga_cache);

  if (read_count == 0 || ferror(fp_gga_cache)) {

    if (++ctx->gga_error_count >= MAX_GGA_UPLOAD_READ_ERRORS) {
      piksi_log(LOG_SBP|LOG_ERR, "max number of GGA file read errors exceeded (" NMEA_GGA_FILE ")");
    }

    if (ferror(fp_gga_cache)) {
      piksi_log(LOG_WARNING, "error reading '" NMEA_GGA_FILE "': %s", strerror(errno));
    } else {
      piksi_log(LOG_WARNING, "no data while reading '" NMEA_GGA_FILE "'");
    }

    fclose(fp_gga_cache);

    return fill_with_gga_xfer_cache(ctx, buf, buf_size);
  }

  if (ctx->debug) {
    piksi_log(LOG_DEBUG, "'" NMEA_GGA_FILE "' read count: %lu", read_count);
  }

  size_t gga_str_len = read_count;

  // Null terminate so trim_crlf won't walk off end of buffer
  buf[read_count] = '\0';
  trim_crlf(buf, &gga_str_len);

  ctx->gga_error_count = 0;

  fclose(fp_gga_cache);
  cache_gga_xfer_buffer(ctx, buf, buf_size, gga_str_len);

  return gga_str_len;
}

static size_t network_upload_write(char *buf, size_t size, size_t n, void *data)
{
  network_context_t *ctx = data;

  time_t now = time(NULL);

  if (now - ctx->last_xfer_time < ctx->gga_xfer_secs) {
    if (ctx->debug) {
      piksi_log(LOG_DEBUG, "Last GGA upload too recent, pausing");
    }
    return CURL_READFUNC_PAUSE;
  }

  char gga_string[256] = {0};
  size_t byte_count = fetch_gga_buffer(ctx, gga_string, sizeof(gga_string) - 1);

  if (byte_count == 0) {
    return CURL_READFUNC_PAUSE;
  }

  size_t header_size = 0;

  if (ctx->gga_rev1) {
    header_size = snprintf(buf, size*n, "%s\r\n", gga_string);
  } else {
    header_size = snprintf(buf, size*n, "Ntrip-GGA: %s\r\n", gga_string);
  }

  if ( header_size >= size*n ) {
    sbp_log(LOG_ERR|LOG_SBP, "%s: unexpected buffer error building GGA string (%s:%d)",
            __FUNCTION__, __FILE__, __LINE__);
    return CURL_READFUNC_PAUSE;
  }

  if (ctx->debug) {
    char gga_string_log[256] = {0};
    strncpy(gga_string_log, buf, sizeof(gga_string_log) - 1);
    trim_crlf(gga_string_log, NULL);
    piksi_log(LOG_DEBUG, "Sending up GGA data: '%s'", gga_string_log);
  }

  ctx->last_xfer_time = now;

  return header_size;
}

static void service_control_fifo(network_context_t *ctx)
{
  if (!ctx->control_fifo_info.configured) {
    return;
  }

  long response_code = 0;
  curl_easy_getinfo(ctx->curl, CURLINFO_RESPONSE_CODE, &response_code);

  if (response_code == 0) {
    if (ctx->response_code != 0) {
      // Return the most recent response code
      response_code = ctx->response_code;
    }
  }

  if (ctx->control_fifo_info.req_read_fd < 0) {
    ctx->control_fifo_info.req_read_fd = open(ctx->control_fifo_info.req_path, O_RDONLY|O_NONBLOCK);
    if (ctx->control_fifo_info.req_read_fd < 0) {
      piksi_log(LOG_WARNING, "%s: error opening request FIFO: %s (%d)", __FUNCTION__, strerror(errno), errno);
      return;
    }
  }

  char cmd[1] = { 0 };
  ssize_t rc = read(ctx->control_fifo_info.req_read_fd, cmd, sizeof(cmd));
  if (rc <= 0) {
    if (rc < 0 && errno != EAGAIN) {
      piksi_log(LOG_WARNING, "%s: error reading from FIFO: %s (%d)", __FUNCTION__, strerror(errno), errno);
    }
    return;
  }

  if (ctx->debug)
    piksi_log(LOG_DEBUG, "%s: got command '%c'", __FUNCTION__, cmd[0]);

  char status_buf[4] = " -1";

  if (cmd[0] != CONTROL_COMMAND_STATUS[0]) {
    piksi_log(LOG_WARNING, "%s: received invalid command '%c'", __FUNCTION__, cmd[0]);
  }

  if (ctx->control_fifo_info.rep_write_fd < 0) {
    ctx->control_fifo_info.rep_write_fd = open(ctx->control_fifo_info.rep_path, O_WRONLY);
    if (ctx->control_fifo_info.rep_write_fd < 0) {
      piksi_log(LOG_WARNING, "%s: error opening response FIFO: %s (%d)", __FUNCTION__, strerror(errno), errno);
      return;
    }
  }

  size_t c = snprintf(status_buf, sizeof(status_buf), "%03ld", response_code);
  if (ctx->debug)
    piksi_log(LOG_DEBUG, "%s: HTTP response code: %d", __FUNCTION__, response_code);

  if (c >= sizeof(status_buf)) {
    piksi_log(LOG_WARNING, "%s: HTTP response code too large: %d", __FUNCTION__, response_code);
  }
  ssize_t wc = write(ctx->control_fifo_info.rep_write_fd, status_buf, sizeof(status_buf) - 1);
  if (wc < 0) {
    piksi_log(LOG_WARNING, "%s: error writing to FIFO: %d", __FUNCTION__, strerror(errno));
  }
}

/**
 * Abort the connection if we make no progress for the last 30 intervals
 */
static int network_progress_check(network_context_t *ctx, curl_off_t bytes)
{
  service_control_fifo(ctx);

  if (ctx->shutdown_signaled) {
    ctx->stall_count = 0;
    return -1;
  }

  if (ctx->cycle_connection_signaled) {

    sbp_log(LOG_WARNING, "forced re-connect requested");

    ctx->cycle_connection_signaled = false;
    ctx->stall_count = 0;

    return -1;
  }

  if (ctx->bytes_transfered == bytes) {
    if (ctx->stall_count++ > MAX_STALLED_INTERVALS) {

      log_with_rate_limit(ctx, LOG_WARNING, "connection stalled");
      ctx->stall_count = 0;

      return -1;
    }
  } else {
    ctx->bytes_transfered = bytes;
    ctx->stall_count = 0;
  }

  if (ctx->gga_xfer_secs > 0) {
    time_t now = time(NULL);
    if (now - ctx->last_xfer_time >= ctx->gga_xfer_secs) {
      curl_easy_pause(ctx->curl, CURLPAUSE_CONT);
    }
  }

  return 0;
}

static int network_download_progress(void *data, curl_off_t dltot, curl_off_t dlnow, curl_off_t ultot, curl_off_t ulnow)
{
  (void)dltot;
  (void)ultot;
  (void)ulnow;

  network_context_t *ctx = data;

  curl_off_t delta = dlnow - ctx->bytes_transfered;
  if (ctx->debug && delta > 0) {
    piksi_log(LOG_DEBUG, "down bytes: now=%lld, prev=%lld, delta=%lld, count %lld", dlnow, ctx->bytes_transfered, delta, ctx->stall_count);
  }

  return network_progress_check(ctx, dlnow);
}

static int network_upload_progress(void *data, curl_off_t dltot, curl_off_t dlnow, curl_off_t ultot, curl_off_t ulnow)
{
  (void)dltot;
  (void)dlnow;
  (void)ultot;

  network_context_t *ctx = data;

  if (ctx->debug) {
    piksi_log(LOG_DEBUG, "up bytes (%lld) %lld count %lld", ulnow, ctx->bytes_transfered, ctx->stall_count);
  }

  return network_progress_check(ctx, ulnow);
}

/**
 * @brief We attempt to void excessive buffering by configuring a small
 *        receive buffer to incoming NTRIP data.
 */
static void configure_recv_buffer(int fd, bool debug) {

  int recvbufsize = RECV_BUFFER_SIZE;
  socklen_t recvbufsize_size = sizeof(recvbufsize);

  // Probably the closest thing we have to controlling how big the TCP rwnd
  //   and cwnd will get?
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvbufsize, recvbufsize_size) < 0) {
    piksi_log(LOG_ERR, "error configuring receive buffer, setsockopt error %d", errno);
  }

  if (debug) {
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &recvbufsize, &recvbufsize_size) < 0) {
      piksi_log(LOG_DEBUG, "unable to read configured buffer size, getsockopt error %d", errno);
    } else {
      piksi_log(LOG_DEBUG, "internal receive buffer size: %d", recvbufsize);
    }
  }
}

static int network_sockopt(void *data, curl_socket_t fd, curlsocktype purpose)
{
  network_context_t *ctx = (network_context_t*)data;
  ctx->socket_fd = fd;

#ifdef TCP_USER_TIMEOUT
  if (purpose == CURLSOCKTYPE_IPCXN) {
    unsigned int timeout = 20000;
    int ret = setsockopt(fd, SOL_TCP, TCP_USER_TIMEOUT, &timeout, sizeof(timeout));
    if (ret < 0) {
      piksi_log(LOG_ERR, "setsockopt error %d", errno);
      return CURL_SOCKOPT_ERROR;
    }
  }
#else
  (void)fd;
  (void)purpose;
#endif

  configure_recv_buffer(fd, ctx->debug);

  return CURL_SOCKOPT_OK;
}

static CURL *network_setup(network_context_t* ctx)
{
  ctx->shutdown_signaled = false;

  ctx->last_xfer_time = 0;
  ctx->bytes_transfered = 0;
  ctx->stall_count = 0;

  CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
  if (code != CURLE_OK) {
    piksi_log(LOG_ERR, "global init %d", code);
    return NULL;
  }

  CURL *curl = curl_easy_init();

  if (curl == NULL) {
    piksi_log(LOG_ERR, "init");
    curl_global_cleanup();
    return NULL;
  }

  ctx->curl = curl;

  return curl;
}

static void network_teardown(CURL *curl)
{
  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

static int network_response_code_check(network_context_t* ctx)
{
  int result = 0;
  if (ctx->response_code_check != NULL) {
    result = ctx->response_code_check(ctx);
  }
  return result;
}

static void log_with_rate_limit(network_context_t* ctx, int priority, const char *format, ...)
{
  time_t current_time = time(NULL);
  time_t last_error_delta = current_time - ctx->last_curl_error_time;

  int facpri = priority;

  if (last_error_delta >= ERROR_REPORTING_INTERVAL && ctx->report_errors) {
    facpri =  LOG_SBP|LOG_WARNING;
    ctx->last_curl_error_time = current_time;
  }

  va_list ap;
  va_start(ap, format);

  piksi_vlog(facpri, format, ap);

  va_end(ap);
}


static void network_request(network_context_t* ctx, CURL *curl)
{
  char error_buf[CURL_ERROR_SIZE];

  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER,       error_buf);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 15000L);
  curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 0L);
  curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT,     1L);
  curl_easy_setopt(curl, CURLOPT_FORBID_REUSE,      1L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE,     1L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL,     5L);
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE,      20L);
  curl_easy_setopt(curl, CURLOPT_BUFFERSIZE,        RECV_BUFFER_SIZE);

  while (true) {

    CURLcode code = curl_easy_perform(curl);

    if (ctx->shutdown_signaled)
      return;

    if (code == CURLE_ABORTED_BY_CALLBACK) {
      if (ctx->debug)
        piksi_log(LOG_DEBUG, "cURL aborted by callback");
      continue;
    }

    long response = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
    ctx->response_code = response;

    if (code != CURLE_OK) {
      log_with_rate_limit(ctx, LOG_WARNING, "curl request (error: %d) \"%s\"", code, error_buf);
    } else {
      if (response != 0) {
        log_with_rate_limit(ctx, LOG_WARNING, "curl request (code: %d) \"%s\"", code, error_buf);
        network_response_code_check(ctx);
      }
    }

    sleep(1);
  }
}

static struct curl_slist *ntrip_init(network_context_t *ctx, CURL *curl)
{
  struct curl_slist *chunk = NULL;

  chunk = curl_slist_append(chunk, "Ntrip-Version: Ntrip/2.0");
  chunk = curl_slist_append(chunk, "Expect:");

  char gga_string[128] = {0};
  size_t gga_len = fetch_gga_buffer(ctx, gga_string, sizeof(gga_string) - 1);

  if (gga_len > 0) {

    char header_buf[256] = {0};

    size_t c = snprintf(header_buf, sizeof(header_buf), "Ntrip-GGA: %s", gga_string);
    assert( c < sizeof(header_buf) );

    curl_slist_append(chunk, header_buf);

  } else {
    piksi_log(LOG_WARNING, "was not able to insert NTRIP GGA header");
  }

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,  "NTRIP swift-ntrip-client/1.0");

  return chunk;
}

static struct curl_slist *skylark_init(CURL *curl)
{
  char uuid_buf[256];
  device_uuid_get(uuid_buf, sizeof(uuid_buf));

  char device_buf[256];
  snprintf(device_buf, sizeof(device_buf), "Device-Uid: %s", uuid_buf);

  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, device_buf);

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,  "skylark-agent/1.0");

  return chunk;
}

static int ntrip_response_code_check(network_context_t *ctx)
{
  if (ctx->response_code == HTTP_RESPONSE_CODE_OK
      && ctx->bytes_transfered == 0) {
    static bool warned = false;
    if (!warned) {
      sbp_log(LOG_WARNING, NTRIP_DROPPED_CONNECTION_WARNING);
      warned = true;
    }
  }
  return 0;
}

void ntrip_download(network_context_t *ctx)
{
  CURL* curl = network_setup(ctx);
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = ntrip_init(ctx, curl);

  if (ctx->gga_xfer_secs > 0) {

    curl_easy_setopt(curl, CURLOPT_UPLOAD,           1L);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,    "GET");
    curl_easy_setopt(curl, CURLOPT_READFUNCTION,     network_upload_write);
    curl_easy_setopt(curl, CURLOPT_READDATA,         ctx);

    chunk = curl_slist_append(chunk, "Transfer-Encoding:");

  } else {
    ctx->response_code_check = ntrip_response_code_check;
  }

  if (strcmp(ctx->username, "") != 0) {
    curl_easy_setopt(curl, CURLOPT_USERNAME,       ctx->username);
    if (ctx->debug) {
      piksi_log(LOG_DEBUG, "username: %s", ctx->username);
    }
  }
  if (strcmp(ctx->password, "") != 0) {
    curl_easy_setopt(curl, CURLOPT_PASSWORD,       ctx->password);
    if (ctx->debug) {
      piksi_log(LOG_DEBUG, "password: %s", ctx->password);
    }
  }
  curl_easy_setopt(curl, CURLOPT_URL,              ctx->url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    network_download_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,        ctx);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, network_download_progress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     ctx);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
  curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION,  network_sockopt);
  curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA,      ctx);

  network_request(ctx, curl);

  curl_slist_free_all(chunk);
  network_teardown(curl);
}

void skylark_download(network_context_t *ctx)
{
  CURL *curl = network_setup(ctx);
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = skylark_init(curl);
  chunk = curl_slist_append(chunk, "Accept: application/vnd.swiftnav.broker.v1+sbp2");

  curl_easy_setopt(curl, CURLOPT_URL,              ctx->url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    network_download_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,        ctx);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, network_download_progress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     ctx);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
  curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION,  network_sockopt);
  curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA,      ctx);

  network_request(ctx, curl);

  curl_slist_free_all(chunk);
  network_teardown(curl);
}

void skylark_upload(network_context_t* ctx)
{
  CURL *curl = network_setup(ctx);
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = skylark_init(curl);
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk, "Content-Type: application/vnd.swiftnav.broker.v1+sbp2");

  curl_easy_setopt(curl, CURLOPT_PUT,              1L);
  curl_easy_setopt(curl, CURLOPT_URL,              ctx->url);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION,     network_upload_read);
  curl_easy_setopt(curl, CURLOPT_READDATA,         ctx);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, network_upload_progress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     ctx);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
  curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION,  network_sockopt);
  curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA,      ctx);

  network_request(ctx, curl);

  curl_slist_free_all(chunk);
  network_teardown(curl);
}
