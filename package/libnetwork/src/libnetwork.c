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

#include <linux/sockios.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>

#include <curl/curl.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "libnetwork.h"

#define LIBRARY_NAME "libnetwork"
#define NUM_LOG_LEVELS 8

/** How large to configure the recv buffer to avoid excessive buffering. */
const size_t RECV_BUFFER_SIZE = 4096;
/** Max number of callbacks from CURLOPT_XFERINFOFUNCTION before we attempt to
 * reconnect to the server */
const curl_off_t MAX_STALLED_INTERVALS = 30;

/** Threshold at which to issue a warning that the pipe will fill up. */
const double PIPE_WARN_THRESHOLD = 0.90;
/** Min amount time between "pipe full" warning messages. */
const double PIPE_WARN_SECS = 5;

typedef struct {
  int fd;
  bool debug;
  curl_socket_t socket_fd;
} network_transfer_t;

typedef struct {
  curl_off_t bytes;
  curl_off_t count;
  bool debug;
} network_progress_t;

static volatile bool shutdown_signaled = false;
static volatile bool cycle_connection_signaled = false;

static time_t last_pipe_warn_time = 0;

void libnetwork_shutdown()
{
  shutdown_signaled = true;
}

void libnetwork_cycle_connection()
{
  cycle_connection_signaled = true;
}

static void sbp_log(int priority, const char *msg_text)
{
  const char *log_args[NUM_LOG_LEVELS] = {"emerg", "alert", "crit",
                                          "error", "warn", "notice",
                                          "info", "debug"};
  FILE *output;
  char cmd_buf[256];
  snprintf(cmd_buf, sizeof(cmd_buf), "sbp_log --%s", log_args[priority]);
  output = popen (cmd_buf, "w");

  if (output == 0) {
    piksi_log(LOG_ERR, "couldn't call sbp_log.");
    return;
  }

  char msg_buf[1024];
  snprintf(msg_buf, sizeof(msg_buf), LIBRARY_NAME ": %s", msg_text);

  fputs(msg_buf, output);

  if (ferror (output) != 0) {
    piksi_log(LOG_ERR, "output to sbp_log failed.");
  }

  if (pclose (output) != 0) {
    piksi_log(LOG_ERR, "couldn't close sbp_log call.");
    return;
  }
}

static void warn_on_pipe_full(int fd, size_t pending_write, bool debug)
{
  int outq_size = 0;

  if (ioctl(fd, FIONREAD, &outq_size) < 0) {
    piksi_log(LOG_ERR, "ioctl error %d", errno);
  }

  int pipe_size = fcntl(fd, F_GETPIPE_SZ);

  if (pipe_size < 0) {
    piksi_log(LOG_ERR, "fcntl error %d (pipe_size=%d)", errno, pipe_size);
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

static void dump_connection_stats(int fd) {

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
  const network_transfer_t *transfer = data;

  if (transfer->debug) {
    dump_connection_stats(transfer->socket_fd);
  }

  warn_on_pipe_full(transfer->fd, size * n, transfer->debug);

  while (true) {

    ssize_t ret = write(transfer->fd, buf, size * n);
    if (ret < 0 && errno == EINTR) {
      continue;
    }

    if (transfer->debug) {
      piksi_log(LOG_DEBUG, "write bytes (%d) %d", size * n, ret);
    }

    return ret;
  }

  return -1;
}

static size_t network_upload_read(char *buf, size_t size, size_t n, void *data)
{
  const network_transfer_t *transfer = data;

  while (true) {
    ssize_t ret = read(transfer->fd, buf, size * n);
    if (ret < 0 && errno == EINTR) {
      continue;
    }

    if (transfer->debug) {
      piksi_log(LOG_DEBUG, "read bytes %d", ret);
    }

    if (ret < 0) {
      return CURL_READFUNC_ABORT;
    }

    return ret;
  }

  return -1;
}

/**
 * Abort the connection if we make no progress for the last 30 intervals
 */
static int network_progress_check(network_progress_t *progress, curl_off_t bytes)
{
  if (shutdown_signaled) {
    progress->count = 0;
    return -1;
  }

  if (cycle_connection_signaled) {

    sbp_log(LOG_WARNING, "forced re-connect requested");

    cycle_connection_signaled = false;
    progress->count = 0;

    return -1;
  }

  int rval = 0;

  if (progress->bytes != bytes) {
    progress->count = 0;
  } else if (progress->count++ > MAX_STALLED_INTERVALS) {
    progress->count = 0;
    rval = -1;
  }

  progress->bytes = bytes;

  return rval;
}

static int network_download_progress(void *data, curl_off_t dltot, curl_off_t dlnow, curl_off_t ultot, curl_off_t ulnow)
{
  (void)dltot;
  (void)ultot;
  (void)ulnow;

  network_progress_t *progress = data;

  curl_off_t delta = dlnow - progress->bytes;
  if (progress->debug && delta > 0) {
    piksi_log(LOG_DEBUG, "down bytes: now=%lld, prev=%lld, delta=%lld, count %lld", dlnow, progress->bytes, delta, progress->count);
  }

  return network_progress_check(progress, dlnow);
}

static int network_upload_progress(void *data, curl_off_t dltot, curl_off_t dlnow, curl_off_t ultot, curl_off_t ulnow)
{
  (void)dltot;
  (void)dlnow;
  (void)ultot;

  network_progress_t *progress = data;

  if (progress->debug) {
    piksi_log(LOG_DEBUG, "up bytes (%lld) %lld count %lld", ulnow, progress->bytes, progress->count);
  }

  return network_progress_check(progress, ulnow);
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
  network_transfer_t* transfer = (network_transfer_t*)data;
  transfer->socket_fd = fd;

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

  configure_recv_buffer(fd, transfer->debug);

  return CURL_SOCKOPT_OK;
}

static CURL *network_setup(void)
{
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

  return curl;
}

static void network_teardown(CURL *curl)
{
  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

static void network_request(CURL *curl, network_transfer_t* transfer)
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
  curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION,   network_sockopt);
  curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA,       transfer);

  while (true) {

    CURLcode code = curl_easy_perform(curl);

    if (code == CURLE_ABORTED_BY_CALLBACK) {

      if (shutdown_signaled)
        return;

      continue;
    }

    if (code != CURLE_OK) {
      piksi_log(LOG_ERR, "curl request (%d) \"%s\"", code, error_buf);
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
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
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
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk, "Accept: application/vnd.swiftnav.broker.v1+sbp2");
  chunk = curl_slist_append(chunk, "Content-Type: application/vnd.swiftnav.broker.v1+sbp2");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,  "skylark-agent/1.0");

  return chunk;
}

void ntrip_download(const network_config_t *config)
{
  shutdown_signaled = false;
  logging_init(LIBRARY_NAME "/ntrip_download");

  network_transfer_t transfer = {
    .fd = config->fd,
    .debug = config->debug,
  };

  network_progress_t progress = {
    .bytes = 0,
    .count = 0,
    .debug = config->debug,
  };

  CURL *curl = network_setup();
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = ntrip_init(curl);

  curl_easy_setopt(curl, CURLOPT_URL,              config->url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    network_download_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,        &transfer);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, network_download_progress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &progress);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

  network_request(curl, &transfer);

  curl_slist_free_all(chunk);
  network_teardown(curl);

  logging_deinit();
}

void skylark_download(const network_config_t *config)
{
  shutdown_signaled = false;
  logging_init(LIBRARY_NAME "/skylark_download");

  network_transfer_t transfer = {
    .fd = config->fd,
    .debug = config->debug,
  };

  network_progress_t progress = {
    .bytes = 0,
    .count = 0,
    .debug = config->debug,
  };

  CURL *curl = network_setup();
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = skylark_init(curl);

  curl_easy_setopt(curl, CURLOPT_URL,              config->url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    network_download_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,        &transfer);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, network_download_progress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &progress);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

  network_request(curl, &transfer);

  curl_slist_free_all(chunk);
  network_teardown(curl);

  logging_deinit();
}

void skylark_upload(const network_config_t *config)
{
  shutdown_signaled = false;
  logging_init(LIBRARY_NAME "/skylark_upload");

  network_transfer_t transfer = {
    .fd = config->fd,
    .debug = config->debug,
  };

  network_progress_t progress = {
    .bytes = 0,
    .count = 0,
    .debug = config->debug,
  };

  CURL *curl = network_setup();
  if (curl == NULL) {
    return;
  }

  struct curl_slist *chunk = skylark_init(curl);

  curl_easy_setopt(curl, CURLOPT_PUT,              1L);
  curl_easy_setopt(curl, CURLOPT_URL,              config->url);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION,     network_upload_read);
  curl_easy_setopt(curl, CURLOPT_READDATA,         &transfer);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, network_upload_progress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     &progress);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);

  network_request(curl, &transfer);

  curl_slist_free_all(chunk);
  network_teardown(curl);

  logging_deinit();
}


