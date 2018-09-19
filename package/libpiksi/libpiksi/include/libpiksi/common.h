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

#ifndef LIBPIKSI_COMMON_H
#define LIBPIKSI_COMMON_H

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <libsbp/sbp.h>

// clang-format off
#ifndef SIZET_MAX
#   define SIZET_MAX  ((size_t)(ssize_t)(-1))
#   define SSIZET_MAX  ((ssize_t)(((size_t)1<<(8*sizeof(ssize_t)-1))-1))
#endif
// clang-format on

#endif /* LIBPIKSI_COMMON_H */
