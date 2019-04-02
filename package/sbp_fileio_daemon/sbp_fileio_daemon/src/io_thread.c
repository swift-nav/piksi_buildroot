/*
 * Copyright (C) 2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "io_thread.h"

typedef struct {
  io_thread_work_fn_t work_fn;
  void* ctx;
} io_request_t;

static struct {
  int request_pipe[2];       /** Pipe for receives new IO requests */
  io_request_t *io_requests; /** Ring buffer of incoming io requests */
  size_t max_pending;
  atomic_size_t io_req_index; /** The current first free index of io requests */
  atomic_size_t io_pending;   /** The number of io requests that are pending for the thread */

  pthread_t thread;     /** Write thread object */
  pthread_mutex_t lock; /** Mutex that protects write operations, all other operations (read /
                           readdir / delete) block around this lock */

  sbp_state_t sbp_state;            /** Used to buffer SBP response packets */

  u8 send_buffer[SEND_BUFFER_SIZE]; /** Stores serialized outgoing SBP response packets */
  u32 send_buffer_remaining;        /** Data remaining in `send_buffer` */
  u32 send_buffer_offset;           /** Current unconsumed offset into `send_buffer` */

  sbp_tx_ctx_t *tx_ctx;             /** The context to use for outgoing data */

} write_thread_ctx;


