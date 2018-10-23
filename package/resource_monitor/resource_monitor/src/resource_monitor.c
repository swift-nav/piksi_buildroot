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

#define _GNU_SOURCE

#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/loop.h>

#include "resource_query.h"
#include "sbp.h"

#include "resource_monitor.h"

#define RESOURCE_USAGE_UPDATE_INTERVAL_MS (1000u)

/**
 * @brief used to trigger usage updates
 */
static void update_metrics(pk_loop_t *loop, void *timer_handle, void *context)
{
  (void)loop;
  (void)timer_handle;
  (void)context;

  resq_run_all();
}

static int parse_options(int argc, char *argv[])
{
  const struct option long_opts[] = {
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    default: {
      return 0;
    } break;
    }
  }

  return 0;
}

static int cleanup(int status);

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    return cleanup(EXIT_FAILURE);
  }

  if (!sbp_init(RESOURCE_USAGE_UPDATE_INTERVAL_MS, update_metrics)) {
    return cleanup(EXIT_FAILURE);
  }

  resq_initilize_all();
  sbp_run();

  return cleanup(EXIT_SUCCESS);
}

static int cleanup(int status)
{
  resq_destroy_all();
  sbp_deinit();
  logging_deinit();

  return status;
}
