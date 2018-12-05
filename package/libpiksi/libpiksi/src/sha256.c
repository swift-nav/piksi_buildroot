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

#include <assert.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/sha256.h>
#include <libpiksi/util.h>

int sha256sum_file(const char *filename, char *sha, size_t sha_size)
{
  assert(sha_size >= SHA256SUM_LEN);

  const char *cmd = "sha256sum";
  const char *argv[3] = {"sha256sum", (char *)filename, NULL};
  int ret = run_with_stdin_file(NULL, cmd, argv, sha, sha_size);

  if (ret) {
    piksi_log(LOG_ERR, "sha256sum error (%d)", ret);
  }

  return ret;
}

int sha256sum_cmp(const char *a, const char *b)
{
  assert(strlen(a) == (SHA256SUM_LEN - 1) && strlen(b) == (SHA256SUM_LEN - 1));
  return strncmp(a, b, SHA256SUM_LEN);
}
