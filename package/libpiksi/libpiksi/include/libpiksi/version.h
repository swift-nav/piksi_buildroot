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

#define VERSION_MAXLEN 128
#define VERSION_DEVSTRING_MAXLEN 64

typedef struct piksi_version_s {
  int marketing;
  int major;
  int patch;
  char devstr[VERSION_DEVSTRING_MAXLEN];
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
 *          be "[[DEV ]v]{x}.{y}.{z}[-<devstring>]" where:
 *          Leading non-digit chars before {x} shall be part of the devstring
 *          x = marketing (only digits)
 *          y = major (only digits)
 *          z = patch (only digits)
 *          Possible following devstring shall be stored to devstr member,
 *          a valid tailing devstring shall begin with '-'
 *
 * @return  0 if no errors
 */
int version_parse_str(const char *str, piksi_version_t *ver);

/**
 * @brief   Check if version is development build
 * @details Version is considered as development build if it has a valid devstring,
 *          except if the devstring is only `v`. Meaning that v{x}.{y}.{z}
 *          versions are not considered as development builds.
 *
 * @return  true if development version
 */
bool version_is_dev(const piksi_version_t *ver);

/**
 * @brief   Compare two versions
 * @details This comparison doesn't include devstring comparison
 *
 * @return  > 0 if a newer
 *          = 0 if a and b equal
 *          < 0 if b newer
 */
int version_cmp(const piksi_version_t *a, const piksi_version_t *b);

/**
 * @brief   Compare devstrings of two versions
 *
 * @return  See strcmp documentation
 */
int version_devstr_cmp(const piksi_version_t *a, const piksi_version_t *b);

#ifdef __cplusplus
}
#endif

#endif /* LIBPIKSI_VERSION_H */

/** @} */
