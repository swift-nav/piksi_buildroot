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

#include "sbp.h"
#include <assert.h>
#include <getopt.h>
#include <libpiksi/logging.h>
#include <libpiksi/settings.h>
#include <libsbp/sbp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGRAM_NAME "sbp_j1939_bridge"

#define J1939_SUB_ENDPOINT "ipc:///var/run/sockets/j1939_internal.pub" /* J1939 Internal Out */
#define J1939_PUB_ENDPOINT "ipc:///var/run/sockets/j1939_internal.sub" /* J1939 Internal In */

bool j1939_debug = false;

struct j1939_sbp_state {
  int counter;
};

struct j1939_sbp_state j1939_to_sbp_state;

bool simulator_enabled_watch = false;

pk_endpoint_t *j1939_pub = NULL;

static int notify_simulator_enable_changed(void *context)
{
  (void)context;
  return 0;
}

static int j19392sbp_decode_frame(const u8 *data, const size_t length, void *context)
{
  (void)data;
  (void)length;
  (void)context;
  struct j1939_sbp_state *state = (struct j1939_sbp_state*) context;
  (void)state;
  piksi_log(LOG_ERR, "j19392sbp_decode_frame");
  return 0;
}

static void j1939_reader_handler(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)handle;
  (void)status;

  pk_endpoint_t *j1939_sub_ept = (pk_endpoint_t *)context;
  if (pk_endpoint_receive(j1939_sub_ept, j19392sbp_decode_frame, &j1939_to_sbp_state) != 0) {
    piksi_log(LOG_ERR,
              "%s: error in %s (%s:%d): %s",
              __FUNCTION__,
              "pk_endpoint_receive",
              __FILE__,
              __LINE__,
              pk_endpoint_strerror());
  }
}

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
  };

  const struct option long_opts[] = {
    {"debug", no_argument, 0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_DEBUG: {
      j1939_debug = true;
    } break;

    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }
  return 0;
}

static int cleanup(pk_endpoint_t **j1939_ept_loc, int status);

int main(int argc, char *argv[])
{
  settings_ctx_t *settings_ctx = NULL;
  pk_loop_t *loop = NULL;
  pk_endpoint_t *j1939_sub = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  if (sbp_init() != 0) {
    piksi_log(LOG_ERR, "error initializing SBP");
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  j1939_sub = pk_endpoint_create(J1939_SUB_ENDPOINT, PK_ENDPOINT_SUB);
  if (j1939_sub == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  if (pk_loop_endpoint_reader_add(loop, j1939_sub, j1939_reader_handler, j1939_sub) == NULL) {
    piksi_log(LOG_ERR, "error adding reader");
    exit(cleanup(&j1939_sub, EXIT_FAILURE));
  }

  settings_ctx = sbp_get_settings_ctx();

  settings_add_watch(settings_ctx,
                     "simulator",
                     "enabled",
                     &simulator_enabled_watch,
                     sizeof(simulator_enabled_watch),
                     SETTINGS_TYPE_BOOL,
                     notify_simulator_enable_changed,
                     NULL);

  sbp_run();

  exit(cleanup(&j1939_sub, EXIT_SUCCESS));
}

static int cleanup(pk_endpoint_t **j1939_ept_loc, int status)
{
  pk_endpoint_destroy(j1939_ept_loc);
  sbp_deinit();
  logging_deinit();
  return status;
}
