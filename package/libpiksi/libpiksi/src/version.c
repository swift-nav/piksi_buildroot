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

#include <ctype.h>

#include <libpiksi/logging.h>
#include <libpiksi/version.h>
#include <libpiksi/util.h>

#define DEVICE_FW_VERSION_FILE_PATH "/img_tbl/boot/name"

int version_current_get(piksi_version_t *ver_parsed)
{
  char ver[64];

  if (file_read_string(DEVICE_FW_VERSION_FILE_PATH, ver, sizeof(ver))) {
    return -1;
  }

  if (version_parse_str(ver, ver_parsed)) {
    return -1;
  }

  return 0;
}

int version_current_get_str(char *str, size_t str_size)
{
  return file_read_string(DEVICE_FW_VERSION_FILE_PATH, str, str_size);
}

#define CHECK_TOKEN(version)                                 \
  do {                                                       \
    if (token && str_digits_only(token)) {                   \
      version = atoi(token);                                 \
    } else {                                                 \
      piksi_log(LOG_ERR, "Invalid version string: %s", str); \
      free(digits);                                          \
      return 1;                                              \
    }                                                        \
  } while (0)

int version_parse_str(const char *str, piksi_version_t *ver)
{
  size_t len = strlen(str);
  size_t idx = 0;

  /* Find first digit in the str */
  while (!isdigit(str[idx])) {
    idx++;

    if (idx >= len) {
      piksi_log(LOG_ERR, "Invalid version string: %s", str);
      return 1;
    }
  }

  char *digits = strdup(str + idx);
  char *rest = digits;
  char *token = NULL;

  token = strtok_r(rest, ".", &rest);
  CHECK_TOKEN(ver->marketing);

  token = strtok_r(NULL, ".", &rest);
  CHECK_TOKEN(ver->major);

  token = strtok_r(NULL, ".", &rest);
  CHECK_TOKEN(ver->patch);

  free(digits);

  return 0;
}

int version_cmp(piksi_version_t *a, piksi_version_t *b)
{
  if (a->marketing != b->marketing) {
    return a->marketing - b->marketing;
  }

  if (a->major != b->major) {
    return a->major - b->major;
  }

  return a->patch - b->patch;
}