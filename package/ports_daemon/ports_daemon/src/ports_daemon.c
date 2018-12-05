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

#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings_client.h>

#include "can.h"
#include "ports.h"
#include "protocols.h"
#include "whitelists.h"
#include "serial.h"

#define PROGRAM_NAME "ports_daemon"
#define SETTINGS_METRICS ("settings/" PROGRAM_NAME)

#define PROTOCOL_LIBRARY_PATH "/usr/lib/endpoint_protocols"

static bool debug = false;
static bool can_enabled = false;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMisc options");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_DEBUG = 1,
    OPT_ID_CAN_ENABLE = 2,
  };

  const struct option long_opts[] = {
    {"debug", no_argument, 0, OPT_ID_DEBUG},
    {"can-enable", no_argument, 0, OPT_ID_CAN_ENABLE},
    {0, 0, 0, 0},
  };

  int opt;

  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {

    case OPT_ID_DEBUG: {
      debug = true;
    } break;

    case OPT_ID_CAN_ENABLE: {
      can_enabled = true;
    } break;

    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }

  return 0;
}

static void settings_init(pk_settings_ctx_t *s)
{
  if (whitelists_init(s) != 0) exit(EXIT_FAILURE);

  if (ports_init(s) != 0) exit(EXIT_FAILURE);

  if (serial_init(s) != 0) exit(EXIT_FAILURE);

  if (can_enabled) {
    if (can_init(s) != 0) exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  if (protocols_import(PROTOCOL_LIBRARY_PATH) != 0) {
    piksi_log(LOG_ERR, "error importing protocols");
    logging_deinit();
    exit(EXIT_FAILURE);
  }

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    logging_deinit();
    exit(EXIT_FAILURE);
  }

  bool ret = pk_settings_loop_simple(SETTINGS_METRICS, settings_init);

  logging_deinit();

  if (!ret) exit(EXIT_FAILURE);

  exit(EXIT_SUCCESS);
}
