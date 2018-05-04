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

#include <assert.h>
#include <uv.h>

#include <libpiksi/util.h>
#include <libpiksi/logging.h>

#include <libpiksi/loop.h>

/**
 * @brief Loop Callback Context
 *
 * This holds the user callback and data for use in the local
 * callback handlers which obfuscate the underlying loop APIs
 */
typedef struct pk_callback_ctx_s {
  pk_loop_cb callback;
  void *data;
} pk_callback_ctx_t;

/**
 * @brief Match Callback Context
 *
 * Maps over handles in the loop to match a specific handle
 * and optionally call a supplied callback with data to
 * operate on that handle.
 */
typedef struct match_handle_ctx_s {
  void *handle;
  bool match;
  uv_walk_cb callback;
  void *data;
} match_handle_ctx_t;

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
};

/* Forward declare of static - see definition below */
static void pk_loop_callback_context_destroy(pk_callback_ctx_t **cb_ctx_loc);

/**
 * @brief pk_loop_callback_context_create - factory method for callback contexts
 * @param callback: Piksi loop callback to store
 * @param data: Optionl user data to pass into callback
 * @return a newly created callback context or NULL if allocation failed
 */
static pk_callback_ctx_t * pk_loop_callback_context_create(pk_loop_cb callback, void *data)
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
static pk_loop_t * pk_loop_from_uv_handle(uv_handle_t *handle)
{
  return uv_loop_get_data(uv_handle_get_loop(handle));
}

/**
 * @brief pk_callback_context_from_uv_handle - get callback context associated with a handle
 * @param handle: handle to get context from
 * @return Loop Callback context
 */
static pk_callback_ctx_t * pk_callback_context_from_uv_handle(uv_handle_t *handle)
{
  return uv_handle_get_data(handle);
}

pk_loop_t * pk_loop_create(void)
{
  pk_loop_t *pk_loop = (pk_loop_t *)malloc(sizeof(pk_loop_t));
  if (pk_loop == NULL) {
    piksi_log(LOG_ERR, "error allocating pk_loop");
    goto failure;
  }

  pk_loop->uv_loop = (uv_loop_t *)malloc(sizeof(uv_loop_t));
  if (pk_loop->uv_loop == NULL) {
    piksi_log(LOG_ERR, "error creating uv_loop");
    goto failure;
  }

  if (uv_loop_init(pk_loop->uv_loop) != 0) {
    piksi_log(LOG_ERR, "error initializing uv_loop");
    goto failure;
  }
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
 * we then need to manually walk to loop to remove each individual handle
 * in order to cleanup the loop itself with uv_loop_close()
 * @param uv_loop: loop to cleanup and destroy
 */
static void pk_loop_destroy_uv_loop(uv_loop_t *uv_loop)
{
  if (uv_loop == NULL) {
    return;
  }
  uv_walk(uv_loop, loop_destroy_callback, NULL);
  while(uv_loop_alive(uv_loop)) {
    if (uv_run(uv_loop, UV_RUN_NOWAIT) == 0) {
      break;
    }
    piksi_log(LOG_DEBUG, "Re-running loop to close pending handles");
  }
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
static void signal_handler(uv_signal_t *signal, int signum) {
  uv_handle_t *handle = (uv_handle_t *)signal;
  pk_loop_t *loop = pk_loop_from_uv_handle(handle);
  pk_callback_ctx_t *cb_ctx = pk_callback_context_from_uv_handle(handle);
  assert(signum == pk_loop_get_signal_from_handle(signal));

  if (cb_ctx->callback != NULL) {
    cb_ctx->callback(loop, handle, cb_ctx->data);
  }
}

void * pk_loop_signal_handler_add(pk_loop_t *pk_loop,
                                  int signal,
                                  pk_loop_cb callback,
                                  void *context)
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
 * @brief uv_loop_poll_handler - wrapping callback for uv_poll_t
 * @param poller: poll handle
 * @param status: poll operation status
 * @param events: event that triggered this callback
 */
static void uv_loop_poll_handler(uv_poll_t *poller, int status, int events)
{
  uv_handle_t *handle = (uv_handle_t *)poller;
  pk_loop_t *loop = pk_loop_from_uv_handle(handle);
  pk_callback_ctx_t *cb_ctx = pk_callback_context_from_uv_handle(handle);

  if (status < 0) {
    piksi_log(LOG_ERR, "UV_ERROR %s", uv_strerror(status));
    return;
  }
  if (events & UV_DISCONNECT) {
    piksi_log(LOG_ERR, "uv_poll_event - UV_DISCONNECT");
    return;
  }

  if (cb_ctx->callback != NULL) {
    cb_ctx->callback(loop, handle, cb_ctx->data);
  }
}

void * pk_loop_endpoint_reader_add(pk_loop_t *pk_loop,
                                   pk_endpoint_t *pk_ept,
                                   pk_loop_cb callback,
                                   void *context)
{
  assert(pk_loop != NULL);
  assert(pk_ept != NULL);
  assert(callback != NULL);

  uv_poll_t *uv_poll = (uv_poll_t *)malloc(sizeof(uv_poll_t));
  if (uv_poll == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate uv_poll for reader add");
    goto failure;
  }

  int poll_handle = pk_endpoint_poll_handle_get(pk_ept);
  if (uv_poll_init_socket(pk_loop->uv_loop, uv_poll, poll_handle) != 0) {
    piksi_log(LOG_ERR, "Failed to init uv_poll for reader add");
    goto failure;
  }

  if (uv_poll_start(uv_poll, UV_READABLE | UV_DISCONNECT, uv_loop_poll_handler) != 0) {
    piksi_log(LOG_ERR, "Failed to start uv_poll for reader add");
    goto failure;
  }

  if (pk_loop_add_handle_context((uv_handle_t *)uv_poll, callback, context) != 0) {
    piksi_log(LOG_ERR, "Failed to allocate callback context for reader add");
    goto failure;
  }

  return uv_poll;

failure:
  pk_loop_destroy_uv_handle((uv_handle_t *)uv_poll);
  return NULL;
}

/**
 * @brief match_handle_walk_callback - maps over handles attempting to match
 * This is mostly a helper for handle_is_valid to receive the match context and
 * use the contents to either match the current handle and call the callback or
 * pass
 * @param handle:
 * @param arg:
 */
static void match_handle_walk_callback(uv_handle_t *handle, void *arg)
{
  match_handle_ctx_t *ctx = (match_handle_ctx_t *)arg;
  if (ctx->handle == (void *)handle) {
    if (ctx->callback != NULL) {
      ctx->callback(handle, ctx->data);
    }
    ctx->match = true;
  }
}

bool pk_loop_handle_is_valid(pk_loop_t *pk_loop, void *handle)
{
  assert(pk_loop != NULL);
  match_handle_ctx_t ctx = {
    .handle = handle,
    .match = false,
    .callback = NULL,
    .data = NULL
  };

  uv_walk(pk_loop->uv_loop, match_handle_walk_callback, &ctx);

  return ctx.match;
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
