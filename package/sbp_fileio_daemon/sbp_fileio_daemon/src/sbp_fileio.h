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

#include <libpiksi/sbp_rx.h>
#include <libpiksi/sbp_tx.h>

#include "path_validator.h"

void sbp_fileio_setup(path_validator_t *pv_ctx,
                      bool allow_factory_mtd,
                      bool allow_imageset_bin,
                      sbp_rx_ctx_t *rx_ctx,
                      sbp_tx_ctx_t *tx_ctx);

#endif
