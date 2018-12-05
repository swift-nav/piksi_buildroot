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

#ifndef SWIFTNAV_ENDPOINT_ROUTER_LOAD_H
#define SWIFTNAV_ENDPOINT_ROUTER_LOAD_H

#include "endpoint_router.h"

#ifdef __cplusplus
extern "C" {
#endif

router_t *router_load(const char *filename);
void router_teardown(router_t **router_loc);

#ifdef __cplusplus
}
#endif

#endif /* SWIFTNAV_ENDPOINT_ROUTER_LOAD_H */
