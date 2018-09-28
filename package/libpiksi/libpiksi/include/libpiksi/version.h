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
 * @file    version.h
 * @brief   Piksi Version API.
 *
 * @defgroup    version Version
 * @addtogroup  version
 * @{
 */

#ifndef LIBPIKSI_VERSION_H
#define LIBPIKSI_VERSION_H

typedef struct piksi_version_s {
  int marketing;
  int major;
  int patch;
} piksi_version_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Get current version
 *
 * @return  0 if no errors
 */
int version_current_get(piksi_version_t *ver);

/**
 * @brief   Get current version as str
 * @details Read current version stored on the file system and return it as a
 *          character array
 *
 * @return  0 if no errors
 */
int version_current_get_str(char *str, size_t str_size);

/**
 * @brief   Parse version string
 * @details Parse given version string into piksi_version_t. String format shall
 *          be "[FOO BLAA BLAA v]{x}.{y}.{z}" where:
 *          x = marketing (only digits)
 *          y = major (only digits)
 *          z = patch (only digits)
 *
 * @return  0 if no errors
 */
int version_parse_str(const char *str, piksi_version_t *ver);

/**
 * @brief   Compare two versions
 *
 * @return; > 0 if a newer
 *          = 0 if a and b equal
 *          < 0 if b newer
 */
int version_cmp(piksi_version_t *a, piksi_version_t *b);

#ifdef __cplusplus
}
#endif

#endif /* LIBPIKSI_VERSION_H */

/** @} */
