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

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/util.h>
#include <getopt.h>
#include "sbp_fileio.h"

static const char *pub_endpoint = NULL;
static const char *sub_endpoint = NULL;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("-p, --pub <addr>");
  puts("-s, --sub <addr>");
}

static int parse_options(int argc, char *argv[])
{
  const struct option long_opts[] = {
    {"pub", required_argument, 0, 'p'},
    {"sub", required_argument, 0, 's'},
    {0, 0, 0, 0}
  };

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "p:s:",
                          long_opts, &opt_index)) != -1) {
    switch (c) {

      case 'p': {
        pub_endpoint = optarg;
      }
      break;

      case 's': {
        sub_endpoint = optarg;
      }
      break;


      default: {
        printf("invalid option\n");
        return -1;
      }
      break;
    }
  }

  if ((pub_endpoint == NULL) || (sub_endpoint == NULL)) {
    printf("ZMQ endpoints not specified\n");
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(pub_endpoint, sub_endpoint);
  if (ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  sbp_fileio_setup(sbp_zmq_pubsub_rx_ctx_get(ctx),
                   sbp_zmq_pubsub_tx_ctx_get(ctx));

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

  sbp_zmq_pubsub_destroy(&ctx);
  exit(EXIT_SUCCESS);
}
