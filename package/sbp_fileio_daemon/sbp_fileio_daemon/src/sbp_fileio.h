/*
 * Copyright (C) 2012-2014 Swift Navigation Inc.
 * Contact: Fergus Noble <fergus@swift-nav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_SBP_FILEIO_H
#define SWIFTNAV_SBP_FILEIO_H

#include <libsbp/file_io.h>

#include <libpiksi/sbp_rx.h>
#include <libpiksi/sbp_tx.h>

#include "path_validator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Global flag that enables or disables debug logging
 */
extern bool fio_debug;

#define FIO_LOG_DEBUG(MsgPattern, ...)        \
  if (fio_debug) {                            \
    piksi_log(LOG_DEBUG,                      \
              ("%s: " MsgPattern " (%s:%d)"), \
              __FUNCTION__,                   \
              ##__VA_ARGS__,                  \
              __FILE__,                       \
              __LINE__);                      \
  }

/**
 * Sets the name of this FileIO daemon
 */
extern const char *sbp_fileio_name;

/**
 * Global flag that disables the file descriptor caching
 */
extern bool no_cache;

/**
 * (Hopefully) temporary flag to disable threading.
 */
extern bool disable_threading;

/**
 * A subset of @see sbp_fileio_setup for unit testing purposes.
 */
void sbp_fileio_setup_path_validator(path_validator_t *pv_ctx,
                                     bool allow_factory_mtd,
                                     bool allow_imageset_bin);

/**
 * Setup the process for operation of the fileio daemon.
 *
 * @param name                The name of the FileIO daemon (e.g. internal /
 *                            external) used to name metrics and control files
 * @param loop                The loop that will handle fileio requests
 * @param pv_ctx              The @c path_validator_t object that will check
 *                            if a path is allowed for fileio
 * @param allow_factory_mtd   If pathes from /factory are allowed for fileio requests
 * @param allow_imageset_bin  If pathes for upgrades are allowed, this is currently just
 *                            `/data/image_set.bin`
 * @param rx_ctx              The @c sbp_rx_ctx_t to use for incoming messages
 * @param tx_ctx              The @c sbp_tx_ctx_t to use for outgoing messages
 *
 * @return If the setup suceeded or failed
 */
bool sbp_fileio_setup(const char *name,
                      pk_loop_t *loop,
                      path_validator_t *pv_ctx,
                      bool allow_factory_mtd,
                      bool allow_imageset_bin,
                      sbp_rx_ctx_t *rx_ctx,
                      sbp_tx_ctx_t *tx_ctx);

/**
 * Teardown a named FileIO daemon.
 */
void sbp_fileio_teardown(const char *name);

/**
 * Flush all cached file descriptors to disk.
 */
void sbp_fileio_flush(void);

/**
 * Request that a named daemon flush all cached file descriptors to disk.
 */
bool sbp_fileio_request_flush(const char *name);

/**
 *
 */
bool sbp_fileio_write(const msg_fileio_write_req_t *msg, size_t length, size_t *write_count);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
