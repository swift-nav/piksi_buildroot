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

#define SBP_PUB_ENDPOINT "tcp://127.0.0.1:43011"
#define SBP_SUB_ENDPOINT "tcp://127.0.0.1:43010"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

int main(int argc, char *argv[])
{
  int ret = -1;

  logging_init(PROGRAM_NAME);

  msg_log_t *msg = alloca(SBP_FRAMING_MAX_PAYLOAD_SIZE);
  const static struct option long_options[] = {
    {"emerg", no_argument, NULL, 0},
    {"alert", no_argument, NULL, 1},
    {"crit", no_argument, NULL, 2},
    {"error", no_argument, NULL, 3},
    {"warn", no_argument, NULL, 4},
    {"notice", no_argument, NULL, 5},
    {"info", no_argument, NULL, 6},
    {"debug", no_argument, NULL, 7},
    {0, 0, 0, 0},
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, NULL)) > 0) {
    if ((unsigned)opt > 7) {
      fprintf(stderr, "Invalid argument\n");
      return -1;
    }
    msg->level = opt;
  }

  sbp_tx_ctx_t *ctx = sbp_tx_create(SBP_PUB_ENDPOINT);
  if (ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  /* Delay for long enough for socket thread to sort itself out */
  usleep(100000);

  while (fgets(msg->text,
               SBP_FRAMING_MAX_PAYLOAD_SIZE - offsetof(msg_log_t, text),
               stdin)) {
    sbp_tx_send(ctx, SBP_MSG_LOG, sizeof(*msg) + strlen(msg->text), (u8*)msg);
  }

  sbp_tx_destroy(&ctx);
  logging_deinit();

  return 0;
}
