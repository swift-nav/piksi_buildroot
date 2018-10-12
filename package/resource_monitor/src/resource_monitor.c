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

#define PROGRAM_NAME "resource_monitor"

#define RESOURCE_USAGE_UPDATE_INTERVAL_MS (1000u)

static unsigned int one_hz_tick_count = 0;

/**
 * @brief used to trigger usage updates
 */
static void update_metrics(pk_loop_t *loop, void *timer_handle,
                                void *context) {

  (void)loop;
  (void)timer_handle;
  (void)context;

  if (one_hz_tick_count >= 5) {
    one_hz_tick_count = 0;
    resq_run_all();
  }

  one_hz_tick_count++;
}

static void signal_handler(pk_loop_t *pk_loop, void *handle, void *context) {
  (void)context;
  int signal_value = pk_loop_get_signal_from_handle(handle);
  piksi_log(LOG_ERR | LOG_SBP, "Caught signal: %d", signal_value);

  pk_loop_stop(pk_loop);
}

static int cleanup(pk_loop_t **pk_loop_loc, int status);

static int parse_options(int argc, char *argv[]) {
  const struct option long_opts[] = {
      {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    default: { return 0; } break;
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {

  pk_loop_t *loop = NULL;
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    return cleanup(&loop, EXIT_FAILURE);
  }

  loop = pk_loop_create();
  if (loop == NULL) {
    return cleanup(&loop, EXIT_FAILURE);
  }

  if (pk_loop_signal_handler_add(loop, SIGINT, signal_handler, NULL) == NULL) {
    piksi_log(LOG_ERR, "Failed to add SIGINT handler to loop");
    return cleanup(&loop, EXIT_FAILURE);
  }

  if (pk_loop_timer_add(loop, RESOURCE_USAGE_UPDATE_INTERVAL_MS,
                        update_metrics, NULL) == NULL) {
    return cleanup(&loop, EXIT_FAILURE);
  }

  pk_loop_run_simple(loop);
  piksi_log(LOG_DEBUG, "Resource Daemon: Normal Exit");

  return cleanup(&loop, EXIT_SUCCESS);
}

static int cleanup(pk_loop_t **pk_loop_loc, int status) {

  resq_destroy_all();
  pk_loop_destroy(pk_loop_loc);
  logging_deinit();

  return status;
}
