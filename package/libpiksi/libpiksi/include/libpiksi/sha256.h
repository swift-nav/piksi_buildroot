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
 * @file    sha256.h
 * @brief   Piksi SHA256 tools.
 *
 * @defgroup    sha256 SHA256
 * @addtogroup  sha256
 * @{
 */

#ifndef LIBPIKSI_SHA256_H
#define LIBPIKSI_SHA256_H

/* Including terminating null */
#define SHA256SUM_LEN 65

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Calculate sha256sum for a file
 * @details Initiates sha256sum tool in a child process and puts its output to
 *          to sha. Function will assert if sha_size is less than SHA256SUM_LEN.
 *
 * @param[in]    input_file   sha256sum shall be calculated from this file
 * @param[inout] sha          Output buffer
 * @param[in]    sha_size     Output buffer size
 *
 * @return                    The operation result.
 * @retval 0                  Success
 */
int sha256sum_file(const char *filename, char *sha, size_t sha_size);

/**
 * @brief   Compare two sha sum strings
 * @details Comparison is done using strncmp. Both string lengths shall be
 *          exactly SHA256SUM_LEN including terminating '\0' character.
 *
 * @return                    The operation result.
 * @retval 0                  SHA sums are equal
 */
int sha256sum_cmp(const char *a, const char *b);

#ifdef __cplusplus
}
#endif

#endif /* LIBPIKSI_SHA256_H */

/** @} */