/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file    libnetwork_http2.h
 * @brief   Network HTT2 API.
 *
 * @defgroup    network Network
 * @addtogroup  network
 * @{
 */

#ifndef SWIFTNAV_LIBNETWORK_HTTP2_H
#define SWIFTNAV_LIBNETWORK_HTTP2_H

#include "libnetwork.h"

void skylark_http2(network_context_t *ctx_up, network_context_t *ctx_down);

#endif /* SWIFTNAV_LIBNETWORK_HTTP2_H */
