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

/**
 * @brief Opaque context for a path_validator.
 */
typedef struct path_validator_s path_validator_t;

/**
 * @brief Configuration struct for `path_validator_create`
 */
typedef struct {
  size_t print_buf_size; /**< The size of the print buffer in bytes. */
} path_validator_cfg_t;

/**
 * @brief Create a path validator, only fails if memory cannot be allocated.
 *
 * @details The path_validator object is used to validate a list of paths that
 *   are allowed for some operation, in the case of the fileio daemon, these
 *   paths that are allowed for read/write/list/etc.
 *
 * @param cfg Configuration for the path_validator object, if NULL, default
 *    values will be used.  Pointer not owned, may be ephemeral.
 */
path_validator_t *path_validator_create(path_validator_cfg_t *cfg);

/**
 * @brief Destroys a path validator object and frees all memory associated.
 *
 * @param pctx pointer to a path_validator point, NULL'd on return.
 */
void path_validator_destroy(path_validator_t **pctx);

/**
 * @brief Check if a given path is in the list of allowed path prefixes.
 *
 * @details Traverses the list of paths specified with `path_validator_allow_path`
 *   and checks if any path prefix matches, returns `true` in that case.
 *
 *   Input paths need not be "prefixed" with "/", since the daemon runs at the
 *   root of the filesystem, the path_validator will automatically strip the
 *   "/" to check if the path is allowed.
 *
 *   Input paths must already exist (or their parent directories must exist) for
 *   the path to be considered allowed.
 */
bool path_validator_check(path_validator_t *ctx, const char* path);

/**
 * @brief Add a path prefix to the list of allowed path prefixes.
 *
 * @details The `path` parameter is copied to an internal buffer, and added to
 *   a list owned by the `path_validator` object.
 *
 * @return true if the path could be added, false otherwise (which usually
 *   indicates a lack of sufficient memory).
 */
bool path_validator_allow_path(path_validator_t *ctx, const char* path);

/**
 * @brief Count of the number of allowed path prefixes.
 */
size_t path_validator_allowed_count(path_validator_t *ctx);

/**
 * @brief Returns an internal buffer with a list of allowed path prefixes.
 *
 * @details Formats a list of path prefixes into a user presentable string,
 *   the function will truncate the string if the list of paths does not fit.
 *   The buffer returned is owned by the path_validator object.
 */
const char* path_validator_base_paths(path_validator_t *ctx);

#ifdef __cplusplus
}
#endif

#endif//SWIFTNAV_PATH_VALIDATOR_H
