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

#ifndef __HEALTH_CONTEXT_H
#define __HEALTH_CONTEXT_H

#include <libpiksi/loop.h>
#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/settings_client.h>

/*
 * Shared context for use by health monitors
 */
typedef struct health_ctx_s health_ctx_t;

/*
 * Get debug from health context
 */
bool health_context_get_debug(health_ctx_t *health_ctx);

/*
 * Get loop from health context
 */
pk_loop_t *health_context_get_loop(health_ctx_t *health_ctx);

/*
 * Get sbp_ctx from health context
 */
sbp_pubsub_ctx_t *health_context_get_sbp_ctx(health_ctx_t *health_ctx);

/*
 * Get settings_ctx from health context
 */
pk_settings_ctx_t *health_context_get_settings_ctx(health_ctx_t *health_ctx);

#endif /* __HEALTH_CONTEXT_H */
