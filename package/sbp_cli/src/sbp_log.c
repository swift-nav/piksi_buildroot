/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <getopt.h>
#include <unistd.h>
#include <stddef.h>

#include <libsbp/sbp.h>
#include <libsbp/logging.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_tx.h>

#define PROGRAM_NAME "sbp_log"
#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

int main(int argc, char *argv[])
{
  int level = LOG_INFO;

  const static struct option long_options[] = {
    {"emerg", no_argument, NULL, LOG_EMERG},
    {"alert", no_argument, NULL, LOG_ALERT},
    {"crit", no_argument, NULL, LOG_CRIT},
    {"error", no_argument, NULL, LOG_ERR},
    {"warn", no_argument, NULL, LOG_WARNING},
    {"notice", no_argument, NULL, LOG_NOTICE},
    {"info", no_argument, NULL, LOG_INFO},
    {"debug", no_argument, NULL, LOG_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, NULL)) > 0) {
    if ((unsigned)opt > LOG_DEBUG) {
      fprintf(stderr, "Invalid argument\n");
      return -1;
    }
    level = opt;
  }

  char msg[SBP_FRAMING_MAX_PAYLOAD_SIZE - offsetof(msg_log_t, text)] = {0};

  while (fgets(msg, sizeof(msg), stdin)) {
    sbp_log(level, msg);
  }

  return 0;
}
