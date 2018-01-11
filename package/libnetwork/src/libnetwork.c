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

#include <linux/sockios.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/queue.h>
#include <sys/socket.h>

#include <curl/curl.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "libnetwork.h"

/** How large to configure the recv buffer to avoid excessive buffering. */
const long RECV_BUFFER_SIZE = 4096L;
/** Max number of callbacks from CURLOPT_XFERINFOFUNCTION before we attempt to
 * reconnect to the server */
const curl_off_t MAX_STALLED_INTERVALS = 300;

/** Threshold at which to issue a warning that the pipe will fill up. */
const double PIPE_WARN_THRESHOLD = 0.90;
/** Min amount time between "pipe full" warning messages. */
const double PIPE_WARN_SECS = 5;

typedef struct context_node context_node_t;

struct network_context_s {

  network_type_t type;         /**< The type of the network session */

  int fd;                      /**< The input fd to read from */
  bool debug;                  /**< Set if we are emitted debug information */
  double gga_xfer_secs;        /**< The number of seconds between GGA upload times */

  time_t last_xfer_time;       /**< The last type GGA was uploaded (for NTRIP only) */
  curl_socket_t socket_fd;     /**< The socket we're read/writing from/to */

  CURL *curl;                  /**< The current cURL handle */

  curl_off_t bytes_transfered; /**< Current number of bytes transferred */
  curl_off_t stall_count;      /**< Number of times the bytes transferred have not changed */

  volatile bool shutdown_signaled;
  volatile bool cycle_connection_signaled;

  context_node_t* node;

  char url[4096];
};

struct context_node {
  network_context_t context;
  LIST_ENTRY(context_node) entries;
};

typedef LIST_HEAD(context_nodes_head, context_node) context_nodes_head_t;

context_nodes_head_t context_nodes_head = LIST_HEAD_INITIALIZER(context_nodes_head);
bool context_nodes_head_initialized = false;

static network_context_t empty_context = {
  .type = NETWORK_TYPE_INVALID,
  .fd = -1,
  .debug = false,
  .gga_xfer_secs = -1.0,
  .last_xfer_time = 0,
  .socket_fd = CURL_SOCKET_BAD,
  .curl = NULL,
  .bytes_transfered = 0,
  .stall_count = 0,
  .shutdown_signaled = false,
  .cycle_connection_signaled = false,
  .node = NULL,
  .url = "",
};

#define NMEA_GGA_FILE "/var/run/nmea_GGA"

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

network_context_t* libnetwork_create(network_type_t type)
{
  if (!context_nodes_head_initialized) {
    LIST_INIT(&context_nodes_head);
    context_nodes_head_initialized = true;
  }

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
  free(*ctx);

  *ctx = NULL;
}

network_status_t libnetwork_set_fd(network_context_t* ctx, int fd)
{
  ctx->fd = fd;
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

static void trim_crlf(char* in_buf, char* out_buf, size_t buf_size)
{
  strncpy(out_buf, in_buf, buf_size);
  char* crlf = strstr(out_buf, "\r\n");
  *crlf = '\0';
}

static size_t network_upload_write(char *buf, size_t size, size_t n, void *data)
{
  network_context_t *ctx = data;

  time_t now = time(NULL);

  if (now - ctx->last_xfer_time < ctx->gga_xfer_secs) {
    if (ctx->debug) {
      piksi_log(LOG_DEBUG, "Last transfer too recent, pausing");
    }
    return CURL_READFUNC_PAUSE;
  }

  ctx->last_xfer_time = now;

  FILE *fp_gga_cache = fopen(NMEA_GGA_FILE, "r");

  if (fp_gga_cache == NULL) {
    piksi_log(LOG_WARNING, "failed to open '" NMEA_GGA_FILE "' file");
    return CURL_READFUNC_PAUSE;
  }

  if (ctx->debug) {
    piksi_log(LOG_DEBUG, "CURL provided buffer size: %lu (size: %lu, count: %lu)", size*n, size, n);
  }

  size_t obj_count = fread(buf, size, n, fp_gga_cache);

  if (obj_count == 0) {
    sbp_log(LOG_WARNING, "no data while reading '" NMEA_GGA_FILE "'");
    return 0;
  }

  if (ctx->debug) {
    piksi_log(LOG_DEBUG, "'" NMEA_GGA_FILE "' read count: %lu", obj_count);
  }

  if (ferror(fp_gga_cache)) {
    piksi_log(LOG_WARNING, "error reading '" NMEA_GGA_FILE "': %s", strerror(errno));
    return 0;
  }

  char log_buf[512];
  trim_crlf(buf, log_buf, sizeof(log_buf));

  if (ctx->debug) {
    piksi_log(LOG_DEBUG, "Sending up GGA data: '%s'", log_buf);
  }

  return obj_count*size;
}

/**
 * Abort the connection if we make no progress for the last 30 intervals
 */
static int network_progress_check(network_context_t *ctx, curl_off_t bytes)
{
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

      sbp_log(LOG_WARNING, "connection stalled");
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

    if (code == CURLE_ABORTED_BY_CALLBACK)
      continue;

    if (code != CURLE_OK) {
      piksi_log(LOG_ERR, "curl request (error: %d) \"%s\"", code, error_buf);
    } else {
      long response;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
      piksi_log(LOG_INFO, "curl request code %d", response);
    }

    usleep(1000000);
  }
}

static struct curl_slist *ntrip_init(CURL *curl)
{
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Ntrip-Version: Ntrip/2.0");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,  "NTRIP ntrip-client/1.0");

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

void ntrip_download(network_context_t *ctx)
{
  CURL* curl = network_setup(ctx);
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = ntrip_init(curl);

  if (ctx->gga_xfer_secs > 0) {

    curl_easy_setopt(curl, CURLOPT_UPLOAD,           1L);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,    "GET");
    curl_easy_setopt(curl, CURLOPT_READFUNCTION,     network_upload_write);
    curl_easy_setopt(curl, CURLOPT_READDATA,         ctx);

    chunk = curl_slist_append(chunk, "Transfer-Encoding:");
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

