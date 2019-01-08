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
#include <getopt.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/sbp_pubsub.h>

#include "sbp_fileio.h"

#define PROGRAM_NAME "sbp_fileio_flush"

static bool print_usage = false;

static void usage(char *command)
{
  printf("Usage: %s\n", command);
  puts("");
  puts("Flush any cached data in a sbp_fileio_daemon to disk");
  puts("");
  puts("-h, --help            Print this message");
  puts("-d, --debug           Output debug logging");
  puts("-n, --name            The name of the daemon to flush");
}

static int parse_options(int argc, char *argv[])
{
  /* clang-format off */
  const struct option long_opts[] = {
    {"name",     required_argument, 0, 'n'},
    {"debug",    no_argument,       0, 'd'},
    {"help",     no_argument,       0, 'h'},
    {0, 0, 0, 0}
  };
  /* clang-format on */

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "n:dh", long_opts, &opt_index)) != -1) {
    switch (c) {

    case 'n': {
      sbp_fileio_name = optarg;
    } break;

    case 'h': {
      print_usage = true;
    } break;

    case 'd': {
      fio_debug = true;
    } break;

    default: {
      printf("invalid option\n");
      return -1;
    } break;
    }
  }

  if (sbp_fileio_name == NULL) {
    fprintf(stderr, "A --name parameter must be specified\n");
    return -1;
  }

  return 0;
}


int main(int argc, char *argv[])
{
  int ret = EXIT_FAILURE;
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    goto cleanup;
  }

  if (print_usage) {
    usage(argv[0]);
    goto cleanup;
  }

  if (sbp_fileio_request_flush(sbp_fileio_name)) {
    ret = EXIT_SUCCESS;
  } else {
    ret = EXIT_FAILURE;
  }

cleanup:
  logging_deinit();
  exit(ret);
}
