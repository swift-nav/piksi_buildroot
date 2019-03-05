/*
 * Copyright (C) 2014-2018 Swift Navigation Inc.
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

#include <alloca.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <libsbp/file_io.h>

#include <libpiksi/cast_check.h>
#include <libpiksi/logging.h>
#include <libpiksi/metrics.h>
#include <libpiksi/util.h>

#include "sbp_fileio.h"
#include "path_validator.h"

#define WAIT_FLUSH_RETRY_MS 50

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

#define TIMER_PERIOD_MS 100

#define PURGE_FD_CACHE_PERIOD_MS 1000
#define CHECK_FD_CACHE_EVERY (PURGE_FD_CACHE_PERIOD_MS / TIMER_PERIOD_MS)

#define FLUSH_METRICS_PERIOD 1000
#define FLUSH_METRICS_EVERY (FLUSH_METRICS_PERIOD / TIMER_PERIOD_MS)

#define SEND_BUFFER_SIZE 4096

#define READ 0
#define WRITE 1

#define WQ_WAIT_SLEEP_NS 1
#define WQ_WAIT_START_MS 10
#define WQ_SELECT_SLEEP_MS 100

#define WQ_MAX_PENDING 256 /** This must be a power of 2 */

#define FILEIO_VERSION_NUMBER 0
#define FILEIO_WINDOW_SIZE 256
#define FILEIO_BATCH_SIZE 32

static u16 sbp_sender_id = 0;

static bool allow_factory_mtd = false;
static bool allow_imageset_bin = false;
static void *timer_handle = NULL;

#define TEST_CTRL_FILE_TEMPLATE "/var/run/sbp_fileio_%s/test"
static char sbp_fileio_test_control[PATH_MAX] = {0};

#define FLUSH_PID_FILE_TEMPLATE "/var/run/sbp_fileio_%s/pid"
static char sbp_fileio_pid_file[PATH_MAX] = {0};

#define WAIT_FLUSH_FILE_TEMPLATE "/var/run/sbp_fileio_%s/wait.flush"
static char sbp_fileio_wait_flush_file[PATH_MAX] = {0};

#define IMAGESET_BIN_NAME "upgrade.image_set.bin"

#define FD_CACHE_COUNT 32
#define CACHE_CLOSE_AGE 3

/* extern */ bool fio_debug = false;
/* extern */ bool no_cache = false;

/* extern */ const char *sbp_fileio_name = NULL;
/* extern */ bool disable_threading = false;

#define MI fileio_metrics_indexes
#define MT fileio_metrics_table
#define MR fileio_metrics

/* clang-format off */
PK_METRICS_TABLE(MT, MI,
  PK_METRICS_ENTRY("data/write/bytes",       "per_second",  M_U32,   M_UPDATE_SUM,   M_RESET_DEF,  write_bytes),
  PK_METRICS_ENTRY("data/read/bytes",        "per_second",  M_U32,   M_UPDATE_SUM,   M_RESET_DEF,  read_bytes),
  PK_METRICS_ENTRY("write_queue/wait/count", "per_second",  M_U32,   M_UPDATE_COUNT, M_RESET_DEF,  wq_waits)
 )
/* clang-format on */

static pk_metrics_t *fileio_metrics = NULL;

typedef struct write_request {
  size_t len;
  u8 msg_buffer[SBP_FRAMING_MAX_PAYLOAD_SIZE];
} write_request_t;

static struct {
  int
    request_pipe[2]; /** Used by write thread to receive incoming write requests through select() */
  write_request_t write_requests[WQ_MAX_PENDING]; /** Ring buffer of incoming write requests */
  atomic_bool flush_pending;     /** True if a flush of FileIO write data is pending */
  atomic_size_t write_req_index; /** The current first free index of write requests */
  atomic_size_t writes_pending;  /** The number of write requests that are pending for the thread */

  pthread_t thread;     /** Write thread object */
  pthread_mutex_t lock; /** Mutex that protects write operations, all other operations (read /
                           readdir / delete) block around this lock */

  pthread_cond_t cond; /** If write operations is pending, all other operations will wait until the
                          write queue is empty before performing their operations.

                          **WARNING** this has the limitation that heavy write operations can
                          potentially starve other operations. */

  sbp_state_t sbp_state;            /** Used to buffer SBP response packets */
  u8 send_buffer[SEND_BUFFER_SIZE]; /** Stores serialized outgoing SBP response packets */
  u32 send_buffer_remaining;        /** Data remaining in `send_buffer` */
  u32 send_buffer_offset;           /** Current unconsumed offset into `send_buffer` */
  sbp_tx_ctx_t *tx_ctx;             /** The context to use for outgoing data */

} write_thread_ctx;

typedef struct {
  FILE *fp;
  bool cached;
  size_t *offset;
} open_file_t;

typedef struct {
  FILE *fp;
  char path[PATH_MAX];
  char mode[4];
  time_t opened_at;
  size_t offset;
} fd_cache_t;

enum {
  DENY_MTD_READ = 0,
  NO_MTD_READ,
  ALLOW_MTD_READ,
};

static int allow_mtd_read(const char *path);
static const char *filter_imageset_bin(const char *filename);

static void read_cb(u16 sender_id, u8 len, u8 msg[], void *context);
static void read_dir_cb(u16 sender_id, u8 len, u8 msg[], void *context);
static void remove_cb(u16 sender_id, u8 len, u8 msg[], void *context);
static void write_cb(u16 sender_id, u8 len, u8 msg[], void *context);
static void config_cb(u16 sender_id, u8 len, u8 buf[], void *context);

static off_t read_offset(open_file_t r);

enum {
  OP_INCREMENT,
  OP_ASSIGN,
};

static void update_offset(open_file_t r, off_t offset, int op);

static void flush_fd_cache();
static void timer_handler(pk_loop_t *loop, void *handle, int status, void *context);
static void flush_cached_fd(const char *path, const char *mode);
static open_file_t open_file(const char *path, const char *mode, int oflag, mode_t perm);

static bool test_control_dir(const char *name);

static void flush_output_sbp(void);
static void request_flush_output_sbp(void);

static void stage_output_sbp(u8 msg_type, size_t len, u8 *payload);

static fd_cache_t fd_cache[FD_CACHE_COUNT] = {[0 ...(FD_CACHE_COUNT - 1)] = {NULL, "", "", 0, 0}};

path_validator_t *g_pv_ctx;

open_file_t empty_open_file = (open_file_t){
  .fp = NULL,
  .cached = false,
  .offset = NULL,
};

fd_cache_t empty_cache_entry = (fd_cache_t){
  .fp = NULL,
  .path = "",
  .mode = "",
  .opened_at = -1,
  .offset = 0,
};

static off_t read_offset(open_file_t r)
{
  if (r.offset == NULL) return -1;
  return (off_t)*r.offset;
}

static void update_offset(open_file_t r, off_t offset, int op)
{
  if (r.offset == NULL) return;

  if (op == OP_INCREMENT)
    (*r.offset) += offset;
  else if (op == OP_ASSIGN)
    (*r.offset) = offset;
  else
    PK_LOG_ANNO(LOG_ERR, "invalid operation: %d", op);
}

static void flush_fd_cache()
{
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {

    if (fd_cache[idx].fp == NULL) continue;

    fclose(fd_cache[idx].fp);
    fd_cache[idx] = empty_cache_entry;
  }
}

static void validate_receive_handle(sbp_rx_ctx_t *rx_ctx)
{
  /* **HACK ALERT**
   *
   *   For some reason sbp_rx doesn't see that the endpoint
   *   has become invalid if the socket becomes disconnected,
   *   we workaround this with a hack that periodically checks
   *   if the endpoint for incoming data is still valid and
   *   exit the daemon if it isn't.  This relies on the
   *   service infrastructure to restart the daemon if this
   *   error occurs.
   */

  pk_endpoint_t *ept = sbp_rx_endpoint_get(rx_ctx);

  if (!pk_endpoint_is_valid(ept)) {
    piksi_log(LOG_ERR, "receive endpoint (%p) is invalid, exiting...", ept);
    exit(EXIT_FAILURE);
  }
}

static void timer_handler(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)status;
  (void)handle;

  static int trigger_cache_purge = 0;
  static int trigger_metrics_flush = 0;

  if (++trigger_cache_purge == CHECK_FD_CACHE_EVERY) {
    trigger_cache_purge = 0;
    time_t now = time(NULL);
    for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
      if (fd_cache[idx].fp == NULL) continue;
      if ((now - fd_cache[idx].opened_at) >= CACHE_CLOSE_AGE) {
        fclose(fd_cache[idx].fp);
        fd_cache[idx] = empty_cache_entry;
      }
    }
  }

  if (++trigger_metrics_flush == FLUSH_METRICS_EVERY) {

    trigger_metrics_flush = 0;

    pk_metrics_flush(MR);

    pk_metrics_reset(MR, MI.read_bytes);
    pk_metrics_reset(MR, MI.write_bytes);
    pk_metrics_reset(MR, MI.wq_waits);

    path_validator_flush_metrics(g_pv_ctx);
  }

  sbp_rx_ctx_t *rx_ctx = (sbp_rx_ctx_t *)context;
  validate_receive_handle(rx_ctx);

  pk_loop_timer_reset(timer_handle);
}

/**
 * The `cached` field of the open_file_t type means that
 * the file descriptor was not cached (either the cache
 * is disabled, or there's no more entries available).
 *
 * In this case we need to close the file to preserve
 * the previous write semantics and flush any written
 * data.
 */
static void finalize_non_cached(open_file_t *the_file)
{
  if (!the_file->cached && the_file->fp != NULL) {
    fclose(the_file->fp);
    return;
  }
}

static void flush_cached_fd(const char *path, const char *mode)
{
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (strncmp(fd_cache[idx].path, path, sizeof(fd_cache[idx].path)) == 0
        && strncmp(fd_cache[idx].mode, mode, sizeof(fd_cache[idx].mode)) == 0) {
      FIO_LOG_DEBUG("Flushing cached write fp (index %d): %p, filename: %s, mode: %s, offset: %d",
                    idx,
                    fd_cache[idx].fp,
                    fd_cache[idx].path,
                    fd_cache[idx].mode,
                    fd_cache[idx].offset);
      fclose(fd_cache[idx].fp);
      fd_cache[idx] = empty_cache_entry;
    }
  }
}

static open_file_t open_file(const char *path, const char *mode, int oflag, mode_t perm)
{
  if (!no_cache) {
    for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
      if (strncmp(fd_cache[idx].path, path, sizeof(fd_cache[idx].path)) == 0
          && strncmp(fd_cache[idx].mode, mode, sizeof(fd_cache[idx].mode)) == 0) {
        FIO_LOG_DEBUG("Found cached fp (index %d): %p, filename: %s, mode: %s, offset: %d",
                      idx,
                      fd_cache[idx].fp,
                      fd_cache[idx].path,
                      fd_cache[idx].mode,
                      fd_cache[idx].offset);
        fd_cache[idx].opened_at = time(NULL);
        return (
          open_file_t){.fp = fd_cache[idx].fp, .cached = true, .offset = &fd_cache[idx].offset};
      }
    }
  }
  int fd = open(path, oflag, perm);
  if (fd < 0) return (open_file_t){.fp = NULL, .cached = false, .offset = NULL};
  FILE *fp = fdopen(fd, mode);
  assert(fp != NULL);
  if (no_cache) {
    return (open_file_t){.fp = fp, .cached = false, .offset = NULL};
  }
  bool found_slot = false;
  size_t *offset_ptr = NULL;
  for (size_t idx = 0; idx < FD_CACHE_COUNT; idx++) {
    if (fd_cache[idx].fp == NULL) {
      strncpy(fd_cache[idx].path, path, sizeof(fd_cache[idx].path));
      strncpy(fd_cache[idx].mode, mode, sizeof(fd_cache[idx].mode));
      fd_cache[idx].fp = fp;
      fd_cache[idx].opened_at = time(NULL);
      offset_ptr = &fd_cache[idx].offset;
      found_slot = true;
      break;
    }
  }
  if (!found_slot) {
    piksi_log(LOG_WARNING, "Could not find a cache slot for file descriptor.");
  }
  return (open_file_t){.fp = fp, .cached = found_slot, .offset = offset_ptr};
}

static bool test_control_dir(const char *name)
{
  snprintf_assert(sbp_fileio_test_control,
                  sizeof(sbp_fileio_test_control),
                  TEST_CTRL_FILE_TEMPLATE,
                  name);
  bool success = file_write_string(sbp_fileio_test_control, "");
  if (success) {
    int rc = unlink(sbp_fileio_test_control);
    if (rc < 0) {
      piksi_log(LOG_ERR,
                "unlink of test file '%s' failed: %s",
                sbp_fileio_test_control,
                strerror(errno));
      success = false;
    }
  } else {
    piksi_log(LOG_ERR,
              "creation/write of test file '%s' failed: %s",
              sbp_fileio_test_control,
              strerror(errno));
  }
  return success;
}

void sbp_fileio_setup_path_validator(path_validator_t *pv_ctx,
                                     bool allow_factory_mtd_,
                                     bool allow_imageset_bin_)
{
  g_pv_ctx = pv_ctx;
  assert(g_pv_ctx != NULL);

  allow_factory_mtd = allow_factory_mtd_;
  allow_imageset_bin = allow_imageset_bin_;
}

static void flush_output_sbp(void)
{
  if (write_thread_ctx.send_buffer_offset == 0) {
    return;
  }

  pk_endpoint_t *ept = sbp_tx_endpoint_get(write_thread_ctx.tx_ctx);
  int rc = pk_endpoint_send(ept, write_thread_ctx.send_buffer, write_thread_ctx.send_buffer_offset);

  if (rc != 0) {
    PK_LOG_ANNO(LOG_WARNING, "pk_endpoint_send reported an error: %d", rc);
  }

  /* reset buffer */
  write_thread_ctx.send_buffer_remaining = sizeof(write_thread_ctx.send_buffer);
  write_thread_ctx.send_buffer_offset = 0;
}

static void request_flush_output_sbp(void)
{
  if (atomic_load(&write_thread_ctx.flush_pending)) {
    return;
  }

  atomic_store(&write_thread_ctx.flush_pending, true);

  void *write_req_ptr = NULL;
  int rc = write(write_thread_ctx.request_pipe[WRITE], &write_req_ptr, sizeof(write_req_ptr));

  if (rc != sizeof(write_req_ptr)) {
    PK_LOG_ANNO(LOG_WARNING, "write() called failed: %s (%d)", strerror(errno), errno);
  }
}

static void post_receive_buffer(void *context)
{
  (void)context;

  request_flush_output_sbp();
}

static size_t wq_acquire_index()
{
  return atomic_fetch_add(&write_thread_ctx.write_req_index, 1) % WQ_MAX_PENDING;
}

static void wq_increment_pending_writes()
{
  while (atomic_load(&write_thread_ctx.writes_pending) == WQ_MAX_PENDING) {
    PK_METRICS_UPDATE(MR, MI.wq_waits);
    nanosleep((const struct timespec[]){{0, WQ_WAIT_SLEEP_NS}}, NULL);
  }

  /* Called WITHOUT lock */
  atomic_fetch_add(&write_thread_ctx.writes_pending, 1);
}

static void wq_signal_startup()
{
  /* Called WITH lock held */
  int rc = pthread_cond_signal(&write_thread_ctx.cond);
  assert(rc == 0);
}

static void wq_wait_startup()
{
  /* Called WITH lock held */
  int rc = pthread_cond_wait(&write_thread_ctx.cond, &write_thread_ctx.lock);
  assert(rc == 0);
}

static void wq_decrement_and_signal()
{
  /* Called WITH lock held */

  if (atomic_fetch_sub(&write_thread_ctx.writes_pending, 1) == 1) {
    int rc = pthread_cond_signal(&write_thread_ctx.cond);
    assert(rc == 0);
  }
}

static void wq_wait_empty()
{
  /* Called WITH lock held */

  if (write_thread_ctx.writes_pending == 0) return;

  int rc = pthread_cond_wait(&write_thread_ctx.cond, &write_thread_ctx.lock);
  assert(rc == 0);
}

NESTED_FN_TYPEDEF(void, when_empty_fn_t);

static void wq_run_when_empty(when_empty_fn_t when_empty_fn)
{
  if (disable_threading) {
    when_empty_fn();
    return;
  }

  /*** MUTEX ACQUIRE ***/
  int rc = pthread_mutex_lock(&write_thread_ctx.lock);
  assert(rc == 0);

  wq_wait_empty();
  when_empty_fn();

  /*** MUTEX RELEASE ***/
  rc = pthread_mutex_unlock(&write_thread_ctx.lock);
  assert(rc == 0);
}

NESTED_FN_TYPEDEF(void, with_lock_fn_t);

static void wq_with_lock(with_lock_fn_t with_lock_fn)
{
  /*** MUTEX ACQUIRE ***/
  int rc = pthread_mutex_lock(&write_thread_ctx.lock);
  assert(rc == 0);

  with_lock_fn();

  /*** MUTEX RELEASE ***/
  rc = pthread_mutex_unlock(&write_thread_ctx.lock);
  assert(rc == 0);
}

static void block_all_signals(void)
{
  /* Make sure that this thread will not handle any signals */

  int rc = 0;

  sigset_t sigset_empty;
  sigfillset(&sigset_empty);

  if ((rc = pthread_sigmask(SIG_SETMASK, &sigset_empty, NULL)) != 0) {
    PK_LOG_ANNO(LOG_WARNING, "pthread_sigmask failed: %s (%d)", strerror(rc), rc);
  }
}

static void *write_thread_handler(void *arg)
{
  (void)arg;

  piksi_log(LOG_DEBUG, "write thread starting...");

  block_all_signals();

  int req_read_fd = write_thread_ctx.request_pipe[READ];

  wq_with_lock(NESTED_FN(void, (), { wq_signal_startup(); }));

  for (;;) {

    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(req_read_fd, &fds);

    struct timeval tv = {.tv_usec = MS_TO_US(WQ_SELECT_SLEEP_MS)};

    if (select((req_read_fd + 1), &fds, NULL, NULL, &tv) < 0) {
      if (errno != EBADF) {
        PK_LOG_ANNO(LOG_WARNING, "select failed: %s (%d)", strerror(errno), errno);
      }
      break;
    }

    if (FD_ISSET(req_read_fd, &fds) == 0) {
      /* Time-out expired, continue */
      continue;
    }

    for (;;) {

      void *write_req_ptr = NULL;
      int rc = read(req_read_fd, &write_req_ptr, sizeof(write_req_ptr));

      if (rc <= 0) break;

      if (write_req_ptr == NULL) {

        flush_output_sbp();
        atomic_store(&write_thread_ctx.flush_pending, false);

        continue;
      }

      write_request_t *write_req = (write_request_t *)write_req_ptr;

      bool write_success = false;
      size_t write_count = 0;

      msg_fileio_write_req_t *msg = (msg_fileio_write_req_t *)write_req->msg_buffer;
      msg_fileio_write_resp_t reply = {.sequence = msg->sequence};

      {
        /*** MUTEX ACQUIRE ***/
        rc = pthread_mutex_lock(&write_thread_ctx.lock);
        assert(rc == 0);

        write_success = sbp_fileio_write(msg, write_req->len, &write_count);
        wq_decrement_and_signal();

        /*** MUTEX RELEASE ***/
        rc = pthread_mutex_unlock(&write_thread_ctx.lock);
        assert(rc == 0);
      }

      if (write_success) {
        PK_METRICS_UPDATE(MR, MI.write_bytes, PK_METRICS_VALUE((u32)write_count));
        stage_output_sbp(SBP_MSG_FILEIO_WRITE_RESP, sizeof(reply), (u8 *)&reply);
      }
    }
  }

  piksi_log(LOG_DEBUG, "write thread is stopping...");

  return NULL;
}

/** Setup file IO
 * Registers relevant SBP callbacks for file IO operations.
 */
bool sbp_fileio_setup(const char *name,
                      pk_loop_t *loop,
                      path_validator_t *pv_ctx,
                      bool allow_factory_mtd_,
                      bool allow_imageset_bin_,
                      sbp_rx_ctx_t *rx_ctx,
                      sbp_tx_ctx_t *tx_ctx)
{
  if (!test_control_dir(name)) {
    /* failure logging happens within the function */
    return false;
  }

  snprintf_assert(sbp_fileio_pid_file, sizeof(sbp_fileio_pid_file), FLUSH_PID_FILE_TEMPLATE, name);

  char pid_buffer[128] = {0};
  snprintf_assert(pid_buffer, sizeof(pid_buffer), "%d", getpid());

  if (!file_write_string(sbp_fileio_pid_file, pid_buffer)) {
    piksi_log(LOG_ERR, "write of pid file '%s' failed", sbp_fileio_pid_file);
    return false;
  }

  sbp_fileio_setup_path_validator(pv_ctx, allow_factory_mtd_, allow_imageset_bin_);

  bool cb_registered = true;

  /* clang-format off */
  cb_registered = cb_registered
    && 0 == sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_REQ, read_cb, tx_ctx, NULL);
  cb_registered = cb_registered
    && 0 == sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_DIR_REQ, read_dir_cb, tx_ctx, NULL);
  cb_registered = cb_registered
    && 0 == sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_REMOVE, remove_cb, tx_ctx, NULL);
  cb_registered = cb_registered
    && 0 == sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_WRITE_REQ, write_cb, tx_ctx, NULL);
  cb_registered = cb_registered
    && 0 == sbp_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_CONFIG_REQ, config_cb, tx_ctx, NULL);
  /* clang-format on */

  if (!cb_registered) {
    PK_LOG_ANNO(LOG_ERR, "callback registration failed");
    return false;
  }

  sbp_sender_id = sbp_sender_id_get();

  sbp_state_init(&write_thread_ctx.sbp_state);
  sbp_rx_receive_buffer_cb_set(rx_ctx, post_receive_buffer, NULL);

  write_thread_ctx.tx_ctx = tx_ctx;

  write_thread_ctx.send_buffer_remaining = sizeof(write_thread_ctx.send_buffer);
  write_thread_ctx.send_buffer_offset = 0;

  timer_handle = pk_loop_timer_add(loop, TIMER_PERIOD_MS, timer_handler, rx_ctx);

  if (timer_handle == NULL) {
    PK_LOG_ANNO(LOG_ERR, "timer setup failed");
    return false;
  }

  fileio_metrics = pk_metrics_setup("sbp_fileio", name, MT, COUNT_OF(MT));

  if (fileio_metrics == NULL) {
    return false;
  }

  atomic_init(&write_thread_ctx.write_req_index, 0);
  atomic_init(&write_thread_ctx.writes_pending, 0);
  atomic_init(&write_thread_ctx.flush_pending, false);

  if (pipe2(write_thread_ctx.request_pipe, O_DIRECT) < 0) {
    piksi_log(LOG_ERR,
              "failed to create write thread request pipe: %s (%d)",
              strerror(errno),
              errno);
    return false;
  }

  if (fcntl(write_thread_ctx.request_pipe[READ], F_SETFL, O_NONBLOCK) < 0) {
    piksi_log(LOG_WARNING,
              "failed to set O_NONBLOCK on read end of request pipe: %s (%d)",
              strerror(errno),
              errno);
  }

  if (fcntl(write_thread_ctx.request_pipe[WRITE], F_SETFL, O_NONBLOCK) < 0) {
    piksi_log(LOG_WARNING,
              "failed to set O_NONBLOCK on write end of request pipe: %s (%d)",
              strerror(errno),
              errno);
  }

  if (!disable_threading) {

    int rc = -1;

    rc = pthread_mutex_init(&write_thread_ctx.lock, NULL);
    assert(rc == 0);

    rc = pthread_cond_init(&write_thread_ctx.cond, NULL);
    assert(rc == 0);

    rc = pthread_create(&write_thread_ctx.thread, NULL, write_thread_handler, NULL);
    assert(rc == 0);

    wq_with_lock(NESTED_FN(void, (), { wq_wait_startup(); }));

    nanosleep_autoresume(0, MS_TO_NS(WQ_WAIT_START_MS));

  } else {
    piksi_log(LOG_DEBUG, "threading disabled...");
  }

  return true;
}

void sbp_fileio_teardown(const char *name)
{
  piksi_log(LOG_DEBUG, "starting write thread teardown...");

  snprintf_assert(sbp_fileio_pid_file, sizeof(sbp_fileio_pid_file), FLUSH_PID_FILE_TEMPLATE, name);

  int rc = unlink(sbp_fileio_pid_file);
  if (rc < 0) {
    piksi_log(LOG_ERR, "unlink of pid file '%s' failed: %s", sbp_fileio_pid_file, strerror(errno));
  }

  if (fileio_metrics != NULL) {
    pk_metrics_destroy(&fileio_metrics);
  }

  close(write_thread_ctx.request_pipe[READ]);
  close(write_thread_ctx.request_pipe[WRITE]);

  if (!disable_threading) {
    pthread_join(write_thread_ctx.thread, NULL);
  }
}

void sbp_fileio_flush(void)
{
  snprintf_assert(sbp_fileio_wait_flush_file,
                  sizeof(sbp_fileio_wait_flush_file),
                  WAIT_FLUSH_FILE_TEMPLATE,
                  sbp_fileio_name);

  flush_fd_cache();

  int rc = unlink(sbp_fileio_wait_flush_file);
  if (rc < 0) {
    piksi_log(LOG_ERR,
              "unlink of flag file '%s' failed: %s",
              sbp_fileio_wait_flush_file,
              strerror(errno));
  }
}

static pid_t read_daemon_pid(const char *name)
{
  char pid_buffer[128] = {0};

  snprintf_assert(sbp_fileio_pid_file, sizeof(sbp_fileio_pid_file), FLUSH_PID_FILE_TEMPLATE, name);
  int rc = file_read_string(sbp_fileio_pid_file, pid_buffer, sizeof(pid_buffer));

  if (rc != 0) {
    piksi_log(LOG_ERR, "reading pid file for flush failed");
    return 0;
  }

  unsigned long pid = 0;

  if (!strtoul_all(10, pid_buffer, &pid)) {
    piksi_log(LOG_WARNING,
              "failed to convert pid value to number: %s (from file '%s')",
              pid_buffer,
              sbp_fileio_pid_file);
    return 0;
  }

  return (pid_t)pid;
}

static bool write_flush_flag_file(const char *name)
{
  snprintf_assert(sbp_fileio_wait_flush_file,
                  sizeof(sbp_fileio_wait_flush_file),
                  WAIT_FLUSH_FILE_TEMPLATE,
                  name);

  {
    int fd = open(sbp_fileio_wait_flush_file, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
      piksi_log(LOG_ERR,
                "creating flag file '%s' failed: %s",
                sbp_fileio_wait_flush_file,
                strerror(errno));
      return false;
    }
    close(fd);
  }

  char wait_pid_buffer[128] = {0};

  snprintf_assert(wait_pid_buffer, sizeof(wait_pid_buffer), "%d", getpid());
  file_write_string(sbp_fileio_wait_flush_file, wait_pid_buffer);

  return true;
}

static bool verify_flush_flag_file()
{
  /* Ensure that we can stat the "wait" flag file at least once */

  struct stat wait_stat;
  if (stat(sbp_fileio_wait_flush_file, &wait_stat) < 0) {
    piksi_log(LOG_ERR,
              "calling stat on flag file '%s' failed: %s",
              sbp_fileio_wait_flush_file,
              strerror(errno));
    return false;
  }

  return true;
}

static bool wait_fd_flush_done()
{
  struct stat wait_stat;
  bool success = true;

  for (;;) {
    if (stat(sbp_fileio_wait_flush_file, &wait_stat) < 0) {
      if (errno == ENOENT) {
        /* An expected error, flag file was removed by the fileio daemon so we are done */
        break;
      } else {
        piksi_log(LOG_ERR,
                  "unexpected error while stat'ing flag file '%s': %s",
                  sbp_fileio_wait_flush_file,
                  strerror(errno));
        success = false;
        break;
      }
    } else {
      struct timespec ts = {.tv_nsec = MS_TO_NS(WAIT_FLUSH_RETRY_MS)};
      while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
        /* no-op, just need to retry nanosleep */;
      }
    }
  }

  return success;
}

bool sbp_fileio_request_flush(const char *name)
{
  pid_t pid = read_daemon_pid(name);
  if (pid == 0) return false;

  if (!write_flush_flag_file(name)) return false;

  if (!verify_flush_flag_file()) return false;

  /* Send SIGUSR1 to tell the daemon to flush all file descriptors to disk */
  kill(pid, SIGUSR1);

  if (!wait_fd_flush_done()) return false;

  piksi_log(LOG_INFO, "sbp_fileio_daemon flush for named daemon '%s' done", name);
  return true;
}

static int allow_mtd_read(const char *path)
{
  /* TODO/DAEMON-USERS: Replace this with an SBP message, e.g. give the firmware a handle
   *    to read here instead of just allowing the read?
   */

  if (strcmp(path, "/factory/mtd") != 0) return NO_MTD_READ;

  return allow_factory_mtd ? ALLOW_MTD_READ : DENY_MTD_READ;
}

static const char *filter_imageset_bin(const char *filename)
{
  if (!allow_imageset_bin) return filename;

  if (strcmp(filename, IMAGESET_BIN_NAME) != 0) return filename;

  static char path_buf[PATH_MAX];

  size_t printed = snprintf(path_buf, sizeof(path_buf), "/data/%s", IMAGESET_BIN_NAME);
  assert(printed < sizeof(path_buf));

  return path_buf;
}

/**
 * File read callback. Responds to a SBP_MSG_FILEIO_READ_REQ message.
 *
 * Reads a certain length (up to 255 bytes) from a given offset. Returns the
 * data in a SBP_MSG_FILEIO_READ_RESP message where the message length field
 * indicates how many bytes were succesfully read.
 */
static void read_cb(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;

  int read_result = 0;
  int seek_result = 0;

  open_file_t the_file = empty_open_file;

  msg_fileio_read_req_t *msg = (msg_fileio_read_req_t *)msg_;
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  if ((len <= sizeof(*msg)) || (len == SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio read message!");
    return;
  }

  /* Add a null termination to filename */
  msg_[len] = 0;

  msg_fileio_read_resp_t *reply;
  int readlen = SWFT_MIN(msg->chunk_size, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*reply));
  reply = alloca(sizeof(msg_fileio_read_resp_t) + readlen);
  reply->sequence = msg->sequence;

  FIO_LOG_DEBUG("read request for '%s', seq=%u, off=%u", msg->filename, msg->sequence, msg->offset);

  bool flush_cached = true;
  int st = allow_mtd_read(msg->filename);

  if (st == DENY_MTD_READ
      || (st == NO_MTD_READ && !path_validator_check(g_pv_ctx, msg->filename))) {

    piksi_log(LOG_WARNING,
              "Received FILEIO_READ request for path (%s) outside base directory (%s), ignoring...",
              msg->filename,
              path_validator_base_paths(g_pv_ctx));

    read_result = 0;
    flush_cached = false;

  } else {

    flush_cached_fd(msg->filename, "w");
    the_file = open_file(msg->filename, "r", O_RDONLY, 0);

    if (the_file.fp == NULL) goto done;

    if (read_offset(the_file) != (off_t)msg->offset) {
      seek_result = fseek(the_file.fp, msg->offset, SEEK_SET);
      if (seek_result == 0) {
        update_offset(the_file, msg->offset, OP_ASSIGN);
      } else {
        PK_LOG_ANNO(LOG_ERR, "fseek failed: %s (%d)", strerror(errno), errno);
      }
    }

    if (seek_result == 0) {
      wq_run_when_empty(NESTED_FN(void, (), {
        read_result = fread(&reply->contents, 1, readlen, the_file.fp);
        if (read_result != readlen) {
          PK_LOG_ANNO(LOG_ERR, "fread failed: %s", strerror(errno));
        }
        finalize_non_cached(&the_file);
        flush_cached = false;
      }));
    }
  }

done:
  if (flush_cached) flush_cached_fd(msg->filename, "r");

  update_offset(the_file, read_result, OP_INCREMENT);
  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_READ_RESP, sizeof(*reply) + read_result, (u8 *)reply);

  PK_METRICS_UPDATE(MR, MI.read_bytes, PK_METRICS_VALUE((u32)read_result));
}

/**
 * Directory listing callback. Responds to a SBP_MSG_FILEIO_READ_DIR_REQ
 * message.
 *
 * The offset parameter can be used to skip the first n elements of the file
 * list.
 *
 * Returns a SBP_MSG_FILEIO_READ_DIR_RESP message containing the directory
 * listings as a NULL delimited list.
 */
static void read_dir_cb(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  msg_fileio_read_dir_req_t *msg = (msg_fileio_read_dir_req_t *)msg_;
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  if ((len <= sizeof(*msg)) || (len == SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio read dir message!");
    return;
  }

  /* Add a null termination to dirname */
  msg_[len] = 0;

  DIR *dir;
  struct dirent *dirent;
  u32 offset = msg->offset;
  msg_fileio_read_dir_resp_t *reply = alloca(SBP_FRAMING_MAX_PAYLOAD_SIZE);
  reply->sequence = msg->sequence;

  FIO_LOG_DEBUG("read_dir request for '%s', seq=%u, off=%u",
                msg->dirname,
                msg->sequence,
                msg->offset);

  if (!path_validator_check(g_pv_ctx, msg->dirname)) {
    piksi_log(LOG_WARNING,
              "Received FILEIO_READ_DIR request for path (%s) "
              "outside base directory (%s), ignoring...",
              msg->dirname,
              path_validator_base_paths(g_pv_ctx));
    len = 0;

    /* Check that directory exists */
  } else if ((dir = opendir(msg->dirname)) == NULL) {
    piksi_log(LOG_WARNING,
              "Received FILEIO_READ_DIR request for path (%s) "
              "which does not exist, ignoring...",
              msg->dirname);
    len = 0;

  } else {
    while (offset && (dirent = readdir(dir)))
      offset--;

    len = 0;
    size_t max_len = SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*reply);
    while ((dirent = readdir(dir))) {
      if (strlen(dirent->d_name) > (max_len - len - 1)) break;
      strcpy((char *)reply->contents + len, dirent->d_name);
      len += strlen(dirent->d_name) + 1;
    }

    closedir(dir);
  }

  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_READ_DIR_RESP, sizeof(*reply) + len, (u8 *)reply);
}

/**
 * Remove file callback. Responds to a SBP_MSG_FILEIO_REMOVE message.
 */
static void remove_cb(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  (void)sender_id;

  if ((len < 1) || (len == SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio remove message!");
    return;
  }

  /* Add a null termination to filename */
  msg[len] = 0;

  const char *filename = filter_imageset_bin((char *)msg);

  FIO_LOG_DEBUG("remove request for '%s'", filename);

  if (!path_validator_check(g_pv_ctx, filename)) {
    piksi_log(
      LOG_WARNING,
      "Received FILEIO_REMOVE request for path (%s) outside base directory (%s), ignoring...",
      filename,
      path_validator_base_paths(g_pv_ctx));
    return;
  }

  wq_run_when_empty(NESTED_FN(void, (), {
    flush_cached_fd(filename, "r");
    flush_cached_fd(filename, "w");
    unlink(filename);
  }));
}

bool sbp_fileio_write(const msg_fileio_write_req_t *msg, size_t length, size_t *write_count)
{
  FIO_LOG_DEBUG("write request for '%s', seq=%u, off=%u",
                msg->filename,
                msg->sequence,
                msg->offset);

  const char *filename = filter_imageset_bin(msg->filename);
  if (!path_validator_check(g_pv_ctx, filename)) {
    piksi_log(
      LOG_WARNING,
      "Received FILEIO_WRITE request for path (%s) outside base directory (%s), ignoring...",
      filename,
      path_validator_base_paths(g_pv_ctx));
    return false;
  }

  flush_cached_fd(filename, "r");
  open_file_t r = open_file(filename, "w", O_WRONLY | O_CREAT, 0666);

  if (r.fp == NULL) {
    piksi_log(LOG_ERR, "Error opening %s for write", filename);
    return false;
  }

  bool success = false;

  if (read_offset(r) != (off_t)msg->offset) {
    if (fseek(r.fp, msg->offset, SEEK_SET) == 0) {
      update_offset(r, msg->offset, OP_ASSIGN);
    } else {
      piksi_log(LOG_ERR,
                "Error seeking to offset %d in %s for write: %s (%d)",
                msg->offset,
                filename,
                strerror(errno),
                errno);
      goto cleanup;
    }
  }

  {
    u8 headerlen = sizeof(*msg) + strlen(msg->filename) + 1;
    *write_count = length - headerlen;

    size_t wres = fwrite(((u8 *)msg) + headerlen, 1, *write_count, r.fp);

    if (wres != *write_count) {
      piksi_log(LOG_ERR, "Error writing %d bytes to %s", *write_count, filename);
      goto cleanup;
    }
  }

  update_offset(r, *write_count, OP_INCREMENT);
  success = true;

cleanup:
  if (!success) flush_cached_fd(filename, "w");
  finalize_non_cached(&r);
  return success;
}

static s32 stage_packed_sbp_buffer(u8 *buff, u32 n, void *context)
{
  (void)context;

  u32 len = SWFT_MIN(write_thread_ctx.send_buffer_remaining, n);

  if (len != n) {
    flush_output_sbp();
    len = SWFT_MIN(write_thread_ctx.send_buffer_remaining, n);
  }

  memcpy(&write_thread_ctx.send_buffer[write_thread_ctx.send_buffer_offset], buff, len);

  write_thread_ctx.send_buffer_remaining -= len;
  write_thread_ctx.send_buffer_offset += len;

  return uint32_to_int32(len);
}

static void stage_output_sbp(u8 msg_type, size_t len, u8 *payload)
{
  int status = sbp_send_message(&write_thread_ctx.sbp_state,
                                msg_type,
                                sbp_sender_id,
                                len,
                                payload,
                                stage_packed_sbp_buffer);

  if (status != SBP_OK) {
    piksi_log(LOG_ERR, "error sending SBP message: %d", status);
    return;
  }
}

static void queue_file_write(u8 len, u8 msg[])
{
  wq_increment_pending_writes();

  write_request_t *write_request = &write_thread_ctx.write_requests[wq_acquire_index()];

  write_request->len = len;
  memcpy(write_request->msg_buffer, msg, len);

  void *write_req_ptr = write_request;
  int rc = write(write_thread_ctx.request_pipe[WRITE], &write_req_ptr, sizeof(write_req_ptr));

  if (rc != sizeof(write_req_ptr)) {
    PK_LOG_ANNO(LOG_WARNING, "write() called failed: %s (%d)", strerror(errno), errno);
  }
}

/**
 * Write to file callback. Responds to a SBP_MSG_FILEIO_WRITE_REQ message.
 *
 * Writes a certain length (up to 255 bytes) at a given offset. Returns a copy
 * of the original SBP_MSG_FILEIO_WRITE_RESP message to check integrity of
 * the write.
 */
static void write_cb(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  (void)context;

  msg_fileio_write_req_t *msg = (msg_fileio_write_req_t *)msg_;

  if ((len <= sizeof(*msg) + 2)
      || (strnlen(msg->filename, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*msg))
          == SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*msg))) {
    piksi_log(LOG_WARNING, "Invalid fileio write message!");
    return;
  }

  if (!disable_threading) {
    queue_file_write(len, msg_);
  } else {
    size_t write_count = 0;
    msg_fileio_write_resp_t reply = {.sequence = msg->sequence};
    if (sbp_fileio_write(msg, len, &write_count)) {
      stage_output_sbp(SBP_MSG_FILEIO_WRITE_RESP, sizeof(reply), (u8 *)&reply);
      PK_METRICS_UPDATE(MR, MI.write_bytes, PK_METRICS_VALUE((u32)write_count));
    }
    flush_output_sbp();
  }
}

static void config_cb(u16 sender_id, u8 len, u8 buf[], void *context)
{
  (void)sender_id;
  sbp_tx_ctx_t *tx_ctx = (sbp_tx_ctx_t *)context;

  if ((len < 1) || (len >= SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio remove message!");
    return;
  }

  piksi_log(LOG_DEBUG, "fileio config request");

  msg_fileio_config_req_t *msg = (msg_fileio_config_req_t *)buf;
  u32 seq = msg->sequence;

  msg_fileio_config_resp_t reply = {.sequence = seq,
                                    .fileio_version = FILEIO_VERSION_NUMBER,
                                    .window_size = FILEIO_WINDOW_SIZE,
                                    .batch_size = FILEIO_BATCH_SIZE};

  sbp_tx_send(tx_ctx, SBP_MSG_FILEIO_CONFIG_RESP, sizeof(reply), (u8 *)&reply);
}
