/*
 * Copyright (C) 2017 Swift Navigation Inc.
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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/loop.h>
#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/sbp_rx.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>

#include <libsbp/sbp.h>
#include <libsbp/system.h>
#include <libsbp/logging.h>

#define PROGRAM_NAME "fpga_error_moni"

#define SBP_SUB_ENDPOINT    "ipc:///var/run/sockets/external.pub"  /* SBP External Out */
#define SBP_PUB_ENDPOINT    "ipc:///var/run/sockets/external.sub"  /* SBP External In */

bool print_debug = false;

static void usage(char *command) {
  printf("Usage: %s\n", command);

  puts("\nOptions:");
  puts("\t--debug   Enable debug messages");
}

static int parse_options(int argc, char *argv[]) {
  enum {
    OPT_ID_DEBUG = 1,
  };

  const struct option long_opts[] = {
    {"debug", no_argument, 0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case OPT_ID_DEBUG:
        print_debug = true;
        break;
      default:
        puts("Invalid option");
        return -1;
    }
  }
  return 0;
}

#define MSG_LOG_LEVEL_WARN 4
#define DONE_PIN_FILE "/sys/devices/soc0/amba/f8007000.devcfg/prog_done"

static void info_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) sender_id;
  (void) len;
  (void) context;
  char line[16];
  FILE *fid;

  msg_log_t *msg_log = (msg_log_t*)msg;

  if (msg_log->level < MSG_LOG_LEVEL_WARN) {
    if (NULL != strstr(msg_log->text, "NAP Verification Failed")) {
      fid = fopen(DONE_PIN_FILE, "rt");
      if (NULL == fid) {
        sbp_log(LOG_ERR, "could not open %s", DONE_PIN_FILE);
        return;
      }
      fgets(line, 16, fid);
      fclose(fid); fid = NULL;
      sbp_log(LOG_ERR, "cat %s -> %s", DONE_PIN_FILE, line);
    }
  }
}

int main(int argc, char *argv[]) {
  int status = EXIT_SUCCESS;
  pk_loop_t *loop = NULL;
  sbp_pubsub_ctx_t *ctx = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    status = EXIT_FAILURE;
    goto cleanup;
  }

  sbp_log(LOG_INFO, PROGRAM_NAME " launched");

  loop = pk_loop_create();
  if (loop == NULL) {
    status = EXIT_FAILURE;
    goto cleanup;
  }

  ctx = sbp_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    status = EXIT_FAILURE;
    goto cleanup;
  }

  sbp_rx_ctx_t *rx_ctx = sbp_pubsub_rx_ctx_get(ctx);
  sbp_tx_ctx_t *tx_ctx = sbp_pubsub_tx_ctx_get(ctx);

  if ((NULL == rx_ctx) || (NULL == tx_ctx)) {
    sbp_log(LOG_ERR, "Error initializing SBP!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_rx_attach(rx_ctx, loop) != 0) {
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_rx_callback_register(rx_ctx, SBP_MSG_LOG, info_callback, ctx, NULL) != 0) {
    sbp_log(LOG_ERR, "Error setting SBP_MSG_LOG callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  pk_loop_run_simple(loop);

cleanup:
  sbp_pubsub_destroy(&ctx);
  pk_loop_destroy(&loop);
  logging_deinit();

  return status;
}
