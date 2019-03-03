/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <uv.h>

#include <libpiksi/endpoint.h>
#include <libpiksi/logging.h>
#include <libpiksi/loop.h>
#include <libpiksi/util.h>

#define MSG_BUF_SIZE 128

/**
 * Libuv for some reason gets a file descriptor value of 0 (which appears
 * to be valid) in the docker unit test environment... however, it later
 * asserts that the handle it's operating on is greater than the value of
 * the stderr handle (2).  We work-around this assert manually copying
 * the FD so that libuv will not assert.
 */
/* #define UNIT_TEST_WORKAROUND */

/**
 * @brief Loop Callback Context
 *
 * This holds the user callback and data for use in the local
 * callback handlers which encapsulate the underlying loop APIs
 */
typedef struct pk_callback_ctx_s {
  pk_loop_cb callback;
  void *data;
} pk_callback_ctx_t;

/**
 * @brief Piksi Loop Context
 *
 * The main context used to access loop APIs. Obfuscates libuv
 * currently and has a preallocated timer to support the timeout
 * loop functionality without having to alloc/dealloc handles.
 */
struct pk_loop_s {
  uv_loop_t *uv_loop;
  uv_timer_t *timeout_timer;
  int uv_last_error;
  char uv_error_msg[MSG_BUF_SIZE];
#ifdef UNIT_TEST_WORKAROUND /* see comment at top of file */
  int uv_handle_copy;
#endif
};

/* Forward declare of static - see definition below */
static void pk_loop_callback_context_destroy(pk_callback_ctx_t **cb_ctx_loc);

/**
 * @brief pk_loop_callback_context_create - factory method for callback contexts
 * @param callback: Piksi loop callback to store
 * @param data: Optional user data to pass into callback
 * @return a newly created callback context or NULL if allocation failed
 */
static pk_callback_ctx_t *pk_loop_callback_context_create(pk_loop_cb callback, void *data)
{
  pk_callback_ctx_t *cb_ctx = (pk_callback_ctx_t *)malloc(sizeof(pk_callback_ctx_t));
  if (cb_ctx == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate callback context");
    goto failure;
  }
  cb_ctx->callback = callback;
  cb_ctx->data = data;

  return cb_ctx;

failure:
  pk_loop_callback_context_destroy(&cb_ctx);
  return NULL;
}

/**
 * @brief pk_loop_callback_context_destroy - cleanup for callback contexts
 * @param cb_ctx_loc: address of pointer to previously allocated callback context
 */
static void pk_loop_callback_context_destroy(pk_callback_ctx_t **cb_ctx_loc)
{
  if (cb_ctx_loc == NULL || *cb_ctx_loc == NULL) {
    return;
  }
  free(*cb_ctx_loc);
  *cb_ctx_loc = NULL;
}

/**
 * @brief pk_loop_add_handle_context - add callback context to a handle
 * This is a convenience function that allocates a callback context and
 * associates in with the handle. The context is retrieved from the handle
 * during loop operation to call the appropriate user function + data
 * @param handle: handle that will receive the allocated callback context
 * @param callback: Piksi loop callback
 * @param data: User data
 * @return 0 on success, -1 if allocation failed.
 */
static int pk_loop_add_handle_context(uv_handle_t *handle, pk_loop_cb callback, void *data)
{
  assert(handle != NULL);
  pk_callback_ctx_t *cb_ctx = pk_loop_callback_context_create(callback, data);
  if (cb_ctx == NULL) {
    piksi_log(LOG_ERR, "Create callback context failed in add handle context");
    goto failure;
  }
  uv_handle_set_data(handle, cb_ctx);

  return 0;

failure:
  pk_loop_callback_context_destroy(&cb_ctx);
  return -1;
}

/**
 * @brief pk_loop_from_uv_handle - get Piksi loop the handle is associated with
 * @param handle: handle to get loop from
 * @return Piksi loop context
 */
static pk_loop_t *pk_loop_from_uv_handle(uv_handle_t *handle)
{
  return uv_loop_get_data(uv_handle_get_loop(handle));
}

/**
 * @brief pk_callback_context_from_uv_handle - get callback context associated with a handle
 * @param handle: handle to get context from
 * @return Loop Callback context
 */
static pk_callback_ctx_t *pk_callback_context_from_uv_handle(uv_handle_t *handle)
{
  return uv_handle_get_data(handle);
}

pk_loop_t *pk_loop_create(void)
{
  pk_loop_t *pk_loop = (pk_loop_t *)malloc(sizeof(pk_loop_t));
  if (pk_loop == NULL) {
    piksi_log(LOG_ERR, "error allocating pk_loop");
    goto failure;
  }

  *pk_loop = (pk_loop_t){
    .uv_loop = NULL,
    .timeout_timer = NULL,
    .uv_error_msg = "",
  };

  pk_loop->uv_loop = (uv_loop_t *)malloc(sizeof(uv_loop_t));
  if (pk_loop->uv_loop == NULL) {
    piksi_log(LOG_ERR, "error creating uv_loop");
    goto failure;
  }

  if (uv_loop_init(pk_loop->uv_loop) != 0) {
    piksi_log(LOG_ERR, "error initializing uv_loop");
    goto failure;
  }
#ifdef UNIT_TEST_WORKAROUND /* see comment at top of file */
  pk_loop->uv_handle_copy = pk_loop->uv_loop->backend_fd;
  pk_loop->uv_loop->backend_fd = dup(pk_loop->uv_loop->backend_fd);
#endif
  uv_loop_set_data(pk_loop->uv_loop, pk_loop);

  pk_loop->timeout_timer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
  if (pk_loop->timeout_timer == NULL) {
    piksi_log(LOG_ERR, "error creating timeout timer");
    goto failure;
  }
  uv_handle_set_data((uv_handle_t *)pk_loop->timeout_timer, NULL);

  if (uv_timer_init(pk_loop->uv_loop, pk_loop->timeout_timer) != 0) {
    piksi_log(LOG_ERR, "error initializing timeout timer");
    goto failure;
  }

  pk_loop->uv_error_msg[0] = '\0';

  return pk_loop;

failure:
  pk_loop_destroy(&pk_loop);
  return NULL;
}

/**
 * @brief handle_destroy_callback - cleanup handle after uv_loop unref's it
 * @param handle: handle passed by uv_loop for cleanup
 */
static void handle_destroy_callback(uv_handle_t *handle)
{
  // assumes all handle data is cb_ctx
  pk_callback_ctx_t *cb_ctx = pk_callback_context_from_uv_handle(handle);
  pk_loop_callback_context_destroy(&cb_ctx);
  free(handle);
}

/**
 * @brief pk_loop_destroy_uv_handle - interal handle removal
 * The proper method of removal for any uv_loop associated handle is to
 * call uv_close and pass a function that will be free any allocated
 * memory once the handle has been properly unref'd from the loop. If
 * for some reason the current handle is not referenced we simply destroy
 * it manually.
 * @param handle: handle to remove from loop and destroy
 */
static void pk_loop_destroy_uv_handle(uv_handle_t *handle)
{
  if (handle == NULL) {
    return;
  }
  if (!uv_has_ref(handle)) {
    handle_destroy_callback(handle);
  } else if (!uv_is_closing(handle)) {
    uv_close(handle, handle_destroy_callback);
  }
}

/**
 * @brief loop_destroy_callback - callback used in loop cleanup
 * @param handle: handle passed from uv_walk
 * @param arg: unused but required argument parameter
 */
static void loop_destroy_callback(uv_handle_t *handle, void *arg)
{
  (void)arg;
  pk_loop_destroy_uv_handle(handle);
}

/**
 * @brief pk_loop_destroy_uv_loop - cleanup a uv_loop
 * Contrary to our use case, a uv_loop generally runs until completion
 * as indicated by the natural removal of all handles from the loop
 * from within callbacks. As we use uv_loop_stop() to exit loop operation,
 * we then need to manually walk the loop to remove each individual handle
 * in order to cleanup the loop itself with uv_loop_close()
 * @param uv_loop: loop to cleanup and destroy
 */
static void pk_loop_destroy_uv_loop(uv_loop_t *uv_loop)
{
  if (uv_loop == NULL) {
    return;
  }
  // call destroy on all handles in the loop
  uv_walk(uv_loop, loop_destroy_callback, NULL);
  // run loop to finish handle cleanup, is 'alive' until last handle removed
  while (uv_loop_alive(uv_loop)) {
    if (uv_run(uv_loop, UV_RUN_NOWAIT) == 0) {
      break;
    }
    // Shouldn't need to get here but log if we do
    piksi_log(LOG_DEBUG, "Re-running loop to close pending handles");
  }
#ifdef UNIT_TEST_WORKAROUND /* see comment at top of file */
  fprintf(stderr, "%s: uv_loop->backend_fd = %d\n", __FUNCTION__, uv_loop->backend_fd);
#endif
  uv_loop_close(uv_loop);
  free(uv_loop);
}

void pk_loop_destroy(pk_loop_t **pk_loop_loc)
{
  if (pk_loop_loc == NULL || *pk_loop_loc == NULL) {
    return;
  }

  pk_loop_t *pk_loop = (pk_loop_t *)(*pk_loop_loc);
  pk_loop_destroy_uv_handle((uv_handle_t *)pk_loop->timeout_timer);
  pk_loop_destroy_uv_loop(pk_loop->uv_loop);
#ifdef UNIT_TEST_WORKAROUND /* see comment at top of file */
  close(pk_loop->uv_handle_copy);
#endif
  pk_loop->uv_loop = NULL;
  free(pk_loop);

  *pk_loop_loc = NULL;
}

int pk_loop_get_signal_from_handle(void *handle)
{
  assert(handle != NULL);
  assert(uv_handle_get_type((uv_handle_t *)handle) == UV_SIGNAL);
  return ((uv_signal_t *)handle)->signum;
}

/**
 * @brief signal_handler - wrapping callback for uv_signal_t
 * @param signal: signal handle
 * @param signum: signal that triggered this callback
 */
static void signal_handler(uv_signal_t *signal, int signum)
{
  uv_handle_t *handle = (uv_handle_t *)signal;
  pk_loop_t *loop = pk_loop_from_uv_handle(handle);
  pk_callback_ctx_t *cb_ctx = pk_callback_context_from_uv_handle(handle);
  assert(signum == pk_loop_get_signal_from_handle(signal));

  if (cb_ctx->callback != NULL) {
    cb_ctx->callback(loop, handle, LOOP_SUCCESS, cb_ctx->data);
  }
}

void *pk_loop_signal_handler_add(pk_loop_t *pk_loop, int signal, pk_loop_cb callback, void *context)
{
  assert(pk_loop != NULL);

  uv_signal_t *uv_signal = (uv_signal_t *)malloc(sizeof(uv_signal_t));
  if (uv_signal == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate signal context");
    goto failure;
  }

  if (uv_signal_init(pk_loop->uv_loop, uv_signal) != 0) {
    piksi_log(LOG_ERR, "Failed to init signal context");
    goto failure;
  }

  if (uv_signal_start(uv_signal, signal_handler, signal) != 0) {
    piksi_log(LOG_ERR, "Failed to start signal context");
    goto failure;
  }

  if (pk_loop_add_handle_context((uv_handle_t *)uv_signal, callback, context) != 0) {
    piksi_log(LOG_ERR, "Failed to allocate callback context for signal handler add");
    goto failure;
  }

  return uv_signal;

failure:
  pk_loop_destroy_uv_handle((uv_handle_t *)uv_signal);
  return NULL;
}

/**
 * @brief timer_handler - wrapping callback for uv_timer_t
 * @param timer: timer handle
 */
static void timer_handler(uv_timer_t *timer)
{
  uv_handle_t *handle = (uv_handle_t *)timer;
  pk_loop_t *loop = pk_loop_from_uv_handle(handle);
  pk_callback_ctx_t *cb_ctx = pk_callback_context_from_uv_handle(handle);

  if (cb_ctx->callback != NULL) {
    cb_ctx->callback(loop, handle, LOOP_SUCCESS, cb_ctx->data);
  }
}

void *pk_loop_timer_add(pk_loop_t *pk_loop, u64 period_ms, pk_loop_cb callback, void *context)
{
  assert(pk_loop != NULL);

  uv_timer_t *uv_timer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
  if (uv_timer == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate timer context");
    goto failure;
  }

  if (uv_timer_init(pk_loop->uv_loop, uv_timer) != 0) {
    piksi_log(LOG_ERR, "Failed to init timer context");
    goto failure;
  }

  if (uv_timer_start(uv_timer, timer_handler, period_ms, period_ms) != 0) {
    piksi_log(LOG_ERR, "Failed to start timer context");
    goto failure;
  }

  if (pk_loop_add_handle_context((uv_handle_t *)uv_timer, callback, context) != 0) {
    piksi_log(LOG_ERR, "Failed to allocate callback context for timer handle add");
    goto failure;
  }

  return uv_timer;

failure:
  pk_loop_destroy_uv_handle((uv_handle_t *)uv_timer);
  return NULL;
}

int pk_loop_timer_reset(void *handle)
{
  assert(handle != NULL);

  if (uv_handle_get_type((uv_handle_t *)handle) != UV_TIMER) {
    piksi_log(LOG_ERR, "Invalid handle passed to timer reset: type mismatch");
    return -1;
  }

  if (uv_timer_again((uv_timer_t *)handle) != 0) {
    piksi_log(LOG_ERR, "Could not reset timer");
    return -1;
  }

  return 0;
}

/**
 * @brief uv_loop_poll_handler - wrapping callback for uv_poll_t
 * @param poller: poll handle
 * @param status: poll operation status
 * @param events: event that triggered this callback
 */
static void uv_loop_poll_handler(uv_poll_t *poller, int status, int events)
{
  int loop_status = LOOP_SUCCESS;
  bool remove = false;

  uv_handle_t *handle = (uv_handle_t *)poller;
  pk_loop_t *loop = pk_loop_from_uv_handle(handle);
  pk_callback_ctx_t *cb_ctx = pk_callback_context_from_uv_handle(handle);

  if (status < 0) {
    loop_status |= LOOP_ERROR;
    loop->uv_last_error = status;
    strncpy(loop->uv_error_msg, uv_strerror(status), sizeof(loop->uv_error_msg));
    remove = true;
  }

  if (events & UV_READABLE) {
    loop_status |= LOOP_READ;
  }

  if (events & UV_DISCONNECT) {
    loop_status |= LOOP_DISCONNECTED;
    remove = true;
  }

  if (remove) {
    pk_loop_poll_remove(loop, handle);
  }

  if (cb_ctx->callback != NULL) {
    cb_ctx->callback(loop, handle, loop_status, cb_ctx->data);
  }
}

void *pk_loop_endpoint_reader_add(pk_loop_t *pk_loop,
                                  pk_endpoint_t *pk_ept,
                                  pk_loop_cb callback,
                                  void *context)
{
  assert(pk_loop != NULL);
  assert(pk_ept != NULL);
  assert(callback != NULL);

  int poll_fd = pk_endpoint_poll_handle_get(pk_ept);

  if (poll_fd < 0) {
    PK_LOG_ANNO(LOG_WARNING, "error fetching poll fd");
    return NULL;
  }

  void *poll_handle = pk_loop_poll_add(pk_loop, poll_fd, callback, context);

  if (poll_handle == NULL) {
    PK_LOG_ANNO(LOG_ERR, "error adding poll fd to loop");
    return NULL;
  }

  if (pk_endpoint_loop_add(pk_ept, pk_loop) < 0) {
    PK_LOG_ANNO(LOG_ERR, "error adding loop to endpoint");
    return NULL;
  }

  return poll_handle;
}

void *pk_loop_poll_add(pk_loop_t *pk_loop, int fd, pk_loop_cb callback, void *context)
{
  assert(pk_loop != NULL);
  assert(fd >= 0);
  assert(callback != NULL);

  uv_poll_t *uv_poll = (uv_poll_t *)malloc(sizeof(uv_poll_t));
  if (uv_poll == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate uv_poll for reader add");
    goto failure;
  }

  if (uv_poll_init(pk_loop->uv_loop, uv_poll, fd) != 0) {
    piksi_log(LOG_ERR, "Failed to init uv_poll");
    goto failure;
  }

  if (uv_poll_start(uv_poll, UV_READABLE | UV_DISCONNECT, uv_loop_poll_handler) != 0) {
    piksi_log(LOG_ERR, "Failed to start uv_poll");
    goto failure;
  }

  if (pk_loop_add_handle_context((uv_handle_t *)uv_poll, callback, context) != 0) {
    piksi_log(LOG_ERR, "Failed to allocate callback context for poll add");
    goto failure;
  }

  return uv_poll;

failure:
  pk_loop_destroy_uv_handle((uv_handle_t *)uv_poll);
  return NULL;
}

void pk_loop_poll_remove(pk_loop_t *pk_loop, void *handle)
{
  (void)pk_loop;

  if (!uv_is_closing((uv_handle_t *)handle)) uv_poll_stop((uv_poll_t *)handle);

  pk_loop_remove_handle((uv_handle_t *)handle);
}

int pk_loop_remove_handle(void *handle)
{
  assert(handle != NULL);

  pk_loop_destroy_uv_handle(handle);

  return 0;
}

int pk_loop_run_simple(pk_loop_t *pk_loop)
{
  assert(pk_loop != NULL);
  assert(pk_loop->uv_loop != NULL);

  uv_run(pk_loop->uv_loop, UV_RUN_DEFAULT);

  return 0;
}

/**
 * @brief pk_loop_timeout_callback - callback for timeout timer
 * The timeout timers job is to stop the loop after a specified timeout,
 * so it simply calls uv_stop on the loop
 * @param timer: timer handle
 */
static void pk_loop_timeout_callback(uv_timer_t *timer)
{
  uv_stop(uv_handle_get_loop((uv_handle_t *)timer));
}

int pk_loop_run_simple_with_timeout(pk_loop_t *pk_loop, u32 timeout_ms)
{
  assert(pk_loop != NULL);
  assert(pk_loop->timeout_timer != NULL);

  uv_timer_start(pk_loop->timeout_timer, pk_loop_timeout_callback, timeout_ms, 0);
  pk_loop_run_simple(pk_loop);
  uv_timer_stop(pk_loop->timeout_timer);

  return 0;
}

void pk_loop_stop(pk_loop_t *pk_loop)
{
  uv_stop(pk_loop->uv_loop);
}

const char *pk_loop_last_error(pk_loop_t *pk_loop)
{
  return pk_loop->uv_error_msg;
}

bool pk_loop_match_last_error(pk_loop_t *pk_loop, int system_error)
{
  return pk_loop->uv_last_error == uv_translate_sys_error(system_error);
}

const char *pk_loop_describe_status(int status)
{
  static char buf[MSG_BUF_SIZE] = {0};
  static char buf_read[MSG_BUF_SIZE] = {0};
  static char buf_disco[MSG_BUF_SIZE] = {0};
  static char buf_error[MSG_BUF_SIZE] = {0};
  bool addbar = false;

  if (status == LOOP_UNKNOWN) {
    snprintf(buf, sizeof(buf), "LOOP_UNKNOWN");
    return buf;
  }

  if (status == LOOP_SUCCESS) {
    snprintf(buf, sizeof(buf), "LOOP_SUCCESS");
    return buf;
  }

  if (status & LOOP_READ) {
    snprintf(buf_read, sizeof(buf_read), "%s%sLOOP_READ", addbar ? buf : "", addbar ? "|" : "");
    snprintf(buf, sizeof(buf), "%s", buf_read);
    addbar = true;
  }

  if (status & LOOP_DISCONNECTED) {
    snprintf(buf_disco,
             sizeof(buf_disco),
             "%s%sLOOP_DISCONNECTED",
             addbar ? buf : "",
             addbar ? "|" : "");
    snprintf(buf, sizeof(buf), "%s", buf_disco);
    addbar = true;
  }

  if (status & LOOP_ERROR) {
    snprintf(buf_error, sizeof(buf_error), "%s%sLOOP_ERROR", addbar ? buf : "", addbar ? "|" : "");
    snprintf(buf, sizeof(buf), "%s", buf_error);
  }

  return buf;
}
