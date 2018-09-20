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

/**
 * @file    pk_loop.h
 * @brief   Piksi Loop API.
 *
 * @defgroup    pk_loop Piksi Loop
 * @addtogroup  pk_loop
 * @{
 */

#ifndef LIBPIKSI_LOOP_H
#define LIBPIKSI_LOOP_H

#include <signal.h>
#include <libpiksi/common.h>

#include <libpiksi/endpoint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct  pk_loop_t
 *
 * @brief   Opaque handle for Piksi Loop.
 */
typedef struct pk_loop_s pk_loop_t;

/**
 * @brief   Piksi Loop Callback Signature
 */
typedef void (*pk_loop_cb)(pk_loop_t *loop, void *handle, void *context);

/**
 * @brief   Create a Piksi loop context
 * @details Create a Piksi loop context
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
pk_loop_t *pk_loop_create(void);

/**
 * @brief   Destroy a Piksi loop context
 * @details Destroy a Piksi loop context
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] pk_loop_loc     Double pointer to the context to destroy.
 */
void pk_loop_destroy(pk_loop_t **pk_loop_loc);

/**
 * @brief   Get signal value from signal handler
 * @details Get signal value from signal handler. For use in signal callback only.
 *
 * @param[in] handle        Handle representing the signal.
 *
 * @note    The handle pointer must be associated with a signal or this will assert.
 *
 * @return                  The signal number.
 */
int pk_loop_get_signal_from_handle(void *handle);

/**
 * @brief   Add a signal handler
 * @details Add a signal handler to call the specified callback on the given signal
 *
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 * @param[in] signal        Signal to handle.
 * @param[in] callback      Callback to use.
 * @param[in] context       Pointer to user data that will be passed to callback.
 *
 * @return                  Signal handle if added successfully, otherwise NULL
 */
void *pk_loop_signal_handler_add(pk_loop_t *pk_loop,
                                 int signal,
                                 pk_loop_cb callback,
                                 void *context);

/**
 * @brief   Add a timer
 * @details Add a timer with a callback, the returned handle can be used to reset in other contexts
 *
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 * @param[in] period_ms     Timer callback period in milliseconds.
 * @param[in] callback      Callback to use.
 * @param[in] context       Pointer to user data that will be passed to callback.
 *
 * @return                  Timer handle if added successfully, otherwise NULL
 */
void *pk_loop_timer_add(pk_loop_t *pk_loop, u64 period_ms, pk_loop_cb callback, void *context);

/**
 * @brief   Reset a timer
 * @details Reset a timer using the handle returned during add
 *
 * @param[in] handle        Handle pointer.
 *
 * @return                  The operation result.
 * @retval 0                Timer reset successfully.
 * @retval -1               An error occurred.
 */
int pk_loop_timer_reset(void *handle);

/**
 * @brief   Add a reader for a given Piksi Endpoint
 * @details Add a reader for a given Piksi Endpoint
 *
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 * @param[in] pk_ept        Pointer to the Piksi Endpoint to add.
 * @param[in] callback      Callback to use.
 * @param[in] context       Pointer to user data that will be passed to callback.
 *
 * @return                  Reader handle if added successfully, otherwise NULL
 */
void *pk_loop_endpoint_reader_add(pk_loop_t *pk_loop,
                                  pk_endpoint_t *pk_ept,
                                  pk_loop_cb callback,
                                  void *context);

/**
 * @brief   Add a poll handle for a given file descriptor
 * @details Add a poll handle for a given file descriptor
 *
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 * @param[in] fd            File descriptor to poll for incoming data.
 * @param[in] callback      Callback to use.
 * @param[in] context       Pointer to user data that will be passed to callback.
 *
 * @return                  Poll handle if added successfully, otherwise NULL
 */
void *pk_loop_poll_add(pk_loop_t *pk_loop, int fd, pk_loop_cb callback, void *context);

/**
 * @brief   Remove a callback handle
 * @details Remove a callback handle from the loop, ending any calls from that context.
 *
 * @param[in] handle        Pointer to handle to remove.
 *
 * @return                  The operation result.
 * @retval 0                Handle removed successfully.
 * @retval -1               An error occurred.
 */
int pk_loop_remove_handle(void *handle);

/**
 * @brief   Run a Piksi loop.
 * @details Run a Piksi loop until an error occurs or handler requests exit
 *
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 *
 * @return                  The operation result.
 * @retval 0                Loop completed successfully.
 * @retval -1               An error occurred.
 */
int pk_loop_run_simple(pk_loop_t *pk_loop);

/**
 * @brief   Run a Piksi loop with timeout.
 * @details Run a Piksi loop until an error occurs, handler requests exit or timeout elapses
 *
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 * @param[in] timeout_ms    Timeout in milliseconds.
 *
 * @return                  The operation result.
 * @retval 0                Loop completed successfully.
 * @retval -1               An error occurred.
 */
int pk_loop_run_simple_with_timeout(pk_loop_t *pk_loop, u32 timeout_ms);

/**
 * @brief   Stop Piksi Loop
 * @details Stop Piksi Loop. Used to signal from within a callback handler and return control.
 *
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 */
void pk_loop_stop(pk_loop_t *pk_loop);

#ifdef __cplusplus
}
#endif

#endif /* LIBPIKSI_LOOP_H */

/** @} */
