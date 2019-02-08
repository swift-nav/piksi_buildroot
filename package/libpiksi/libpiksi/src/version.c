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

int version_parse_str(const char *str, piksi_version_t *ver)
{
  size_t len = strlen(str);
  size_t idx = 0;
  char tail[len];
  memset(tail, 0, len);
  memset(ver, 0, sizeof(piksi_version_t));

  /* Find first digit in the str */
  while (!isdigit(str[idx])) {
    idx++;

    if (idx >= len || VERSION_DEVSTRING_MAXLEN < idx) {
      piksi_log(LOG_ERR, "Invalid version string: %s", str);
      return 1;
    }
  }

  memcpy(ver->devstr, str, idx);

  int res = sscanf(str + idx, "%d.%d.%d%s", &ver->marketing, &ver->major, &ver->patch, tail);

  switch (res) {
  case 3: return 0;
  case 4:
    if (strlen(tail) > 0 && tail[0] == '-') {
      snprintf_warn(ver->devstr + idx, VERSION_DEVSTRING_MAXLEN - idx, "%s", tail);
      return 0;
    } else {
      /* Fall to default (erroneous version string) */
    }
    /* fall through */
  default: piksi_log(LOG_ERR, "Invalid version string: %s", str); return 1;
  }
}

bool version_is_dev(const piksi_version_t *ver)
{
  if (0 == strlen(ver->devstr)) {
    return false;
  }

  /* v1.2.3 is not considered as DEV build */
  if (1 == strlen(ver->devstr)) {
    return ('v' != ver->devstr[0]);
  }

  return true;
}

int version_cmp(const piksi_version_t *a, const piksi_version_t *b)
{
  if (a->marketing != b->marketing) {
    return a->marketing - b->marketing;
  }

  if (a->major != b->major) {
    return a->major - b->major;
  }

  return a->patch - b->patch;
}

int version_devstr_cmp(const piksi_version_t *a, const piksi_version_t *b)
{
  return strcmp(a->devstr, b->devstr);
}
