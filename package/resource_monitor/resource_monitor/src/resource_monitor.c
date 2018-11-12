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
#include <libpiksi/settings.h>

#include "resource_query.h"
#include "sbp.h"

#include "resource_monitor.h"

#define DEFAULT_UPDATE_INTERVAL (0u) // Disabled by default
#define TO_MILLISECONDS(X) ((unsigned int)(X)*1000u)

static int resource_monitor_interval_s = DEFAULT_UPDATE_INTERVAL;

/**
 * @brief used to trigger usage updates
 */
static void update_metrics(pk_loop_t *loop, void *timer_handle, int status, void *context)
{
  (void)loop;
  (void)timer_handle;
  (void)status;
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

static int notify_interval_changed(void *context)
{
  (void)context;
  static int previous_interval = -1;

  if (previous_interval != resource_monitor_interval_s) {

    piksi_log(LOG_DEBUG,
              "resource monitor config: %d -> %d",
              previous_interval,
              resource_monitor_interval_s);

    resq_teardown_all();

    if (resource_monitor_interval_s != 0) {
      resq_initialize_all();
      piksi_log(LOG_INFO | LOG_SBP, "resource monitor starting");
    } else if (previous_interval != -1) {
      piksi_log(LOG_INFO | LOG_SBP, "resource monitor stopped");
    }

    previous_interval = resource_monitor_interval_s;
  }

  sbp_update_timer_interval(TO_MILLISECONDS(resource_monitor_interval_s), update_metrics);

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

  if (!sbp_init((u32)resource_monitor_interval_s, update_metrics)) {
    return cleanup(EXIT_FAILURE);
  }

  settings_ctx_t *settings_ctx = sbp_get_settings_ctx();

  settings_register(settings_ctx,
                    "system",
                    "resource_monitor_update_interval",
                    &resource_monitor_interval_s,
                    sizeof(resource_monitor_interval_s),
                    SETTINGS_TYPE_INT,
                    notify_interval_changed,
                    NULL);

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
