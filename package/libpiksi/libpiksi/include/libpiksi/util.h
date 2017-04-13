/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file    util.h
 * @brief   Utilities API.
 *
 * @defgroup    util Util
 * @addtogroup  util
 * @{
 */

#ifndef LIBPIKSI_UTIL_H
#define LIBPIKSI_UTIL_H

#include <libpiksi/common.h>

/**
 * @brief   Get the SBP sender ID for the system.
 * @details Returns the board-specific SBP sender ID.
 *
 * @return                  The SBP sender ID.
 */
u16 sbp_sender_id_get(void);

/**
 * @brief   Get the Device UUID for the system.
 * @details Returns the board-specific UUID.
 *
 * @return                  The operation result.
 * @retval 0                The value was returned successfully.
 * @retval -1               An error occurred.
 */
int device_uuid_get(char *str, size_t str_size);

/**
 * @brief   Run a ZMQ loop ignoring signals.
 * @details Run a ZMQ loop ignoring signals until an error occurs or a handler
 *          returns -1.
 *
 * @param[in] zloop         Pointer to the ZMQ loop to use.
 *
 * @return                  The operation result.
 * @retval 0                A handler returned -1.
 * @retval -1               An error occurred.
 */
int zmq_simple_loop(zloop_t *zloop);

/**
 * @brief   Run a ZMQ loop ignoring signals with timeout.
 * @details Run a ZMQ loop ignoring signals until an error occurs or a handler
 *          returns -1 with timeout.
 *
 * @param[in] zloop         Pointer to the ZMQ loop to use.
 * @param[in] timeout_ms    Timeout in milliseconds.
 *
 * @return                  The operation result.
 * @retval 0                A handler returned -1.
 * @retval -1               An error occurred.
 */
int zmq_simple_loop_timeout(zloop_t *zloop, u32 timeout_ms);

#endif /* LIBPIKSI_UTIL_H */

/** @} */
