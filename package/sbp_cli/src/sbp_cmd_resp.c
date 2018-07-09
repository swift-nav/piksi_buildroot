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

#include <getopt.h>
#include <unistd.h>

#include <libsbp/sbp.h>
#include <libsbp/piksi.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_tx.h>

#define PROGRAM_NAME "sbp_cmb_resp"

#define SBP_PUB_ENDPOINT "ipc:///var/run/sockets/firmware.sub"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nOptions:");
  puts("\t--sequence The sequence number of the MSG_SBP_COMMAND_RESP message");
  puts("\t--status   The status value to include in the MSG_SBP_COMMAND_RESP message");
}

static u32 sequence = 0;
static s32 status = 0;

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_SEQUENCE = 1,
    OPT_ID_STATUS,
  };

  const static struct option long_options[] = {
    {"sequence", required_argument, NULL, OPT_ID_SEQUENCE},
    {"status", required_argument, NULL, OPT_ID_STATUS},
    {0, 0, 0, 0},
  };

  bool sequence_set = false;
  bool status_set = false;

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, NULL)) > 0) {
    switch (opt) {
    case OPT_ID_SEQUENCE:
      sequence = (u32)strtoul(optarg, NULL, 10);
      sequence_set = true;
      break;
    case OPT_ID_STATUS:
      status = (s32)strtol(optarg, NULL, 10);
      status_set = true;
      break;
    default:
      return -1;
    }
  }

  if (!sequence_set || !status_set)
    return -1;

  return 0;
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);
  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  sbp_tx_ctx_t *ctx = sbp_tx_create(SBP_PUB_ENDPOINT);
  if (ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  usleep(100000);

  msg_command_resp_t resp = {
    .sequence = sequence,
    .code = status,
  };

  piksi_log(LOG_INFO, "sending command status: %d, sequence id: %u",
            status, sequence);

  sbp_tx_send(ctx, SBP_MSG_COMMAND_RESP, sizeof(resp), (void*)&resp);

  sbp_tx_destroy(&ctx);
  logging_deinit();

  return 0;
}
