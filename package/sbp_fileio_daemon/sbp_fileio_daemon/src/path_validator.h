/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swift-nav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_PATH_VALIDATOR_H
#define SWIFTNAV_PATH_VALIDATOR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct path_validator_s path_validator_t;

path_validator_t *path_validator_create(void);

void path_validator_destroy(path_validator_t **pctx);

bool path_validator_check(path_validator_t *ctx, const char* path);

void path_validator_allow_path(path_validator_t *ctx, const char* path);
size_t path_validator_allowed_count(path_validator_t *ctx);

#ifdef __cplusplus
}
#endif

#endif//SWIFTNAV_PATH_VALIDATOR_H
