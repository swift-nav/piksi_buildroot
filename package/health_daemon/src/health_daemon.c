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
#include <stdlib.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/loop.h>
#include <libpiksi/settings_client.h>

#include <libsbp/sbp.h>
#include <libsbp/observation.h>
#include <libsbp/navigation.h>
#include <libsbp/system.h>
#include <libsbp/tracking.h>

#include "health_monitor.h"

/* Include custom health monitors here */
#include "baseline_monitor.h"
#include "glo_obs_monitor.h"
#include "glo_bias_monitor.h"
#include "skylark_monitor.h"
#include "ntrip_obs_monitor.h"
#include "gnss_time_monitor.h"
#include "base_num_sats_monitor.h"

#define PROGRAM_NAME "health_daemon"
#define SETTINGS_METRICS_NAME ("settings/" PROGRAM_NAME)

#define SBP_SUB_ENDPOINT "ipc:///var/run/sockets/external.pub" /* SBP External Out */
#define SBP_PUB_ENDPOINT "ipc:///var/run/sockets/external.sub" /* SBP External In */

/* #define ENABLE_SKYLARK_MONITOR */

struct health_ctx_s {
  bool health_debug;
  pk_loop_t *loop;
  sbp_pubsub_ctx_t *sbp_ctx;
  pk_settings_ctx_t *settings_ctx;
};

bool health_context_get_debug(health_ctx_t *health_ctx)
{
  return health_ctx->health_debug;
}

pk_loop_t *health_context_get_loop(health_ctx_t *health_ctx)
{
  return health_ctx->loop;
}

sbp_pubsub_ctx_t *health_context_get_sbp_ctx(health_ctx_t *health_ctx)
{
  return health_ctx->sbp_ctx;
}

pk_settings_ctx_t *health_context_get_settings_ctx(health_ctx_t *health_ctx)
{
  return health_ctx->settings_ctx;
}

static health_ctx_t health_ctx = {
  .health_debug = false,
  .loop = NULL,
  .sbp_ctx = NULL,
  .settings_ctx = NULL,
};

static health_monitor_init_fn_pair_t health_monitor_init_pairs[] = {
  {baseline_threshold_health_monitor_init, baseline_threshold_health_monitor_deinit},
  {glo_obs_timeout_health_monitor_init, glo_obs_timeout_health_monitor_deinit},
  {glo_bias_timeout_health_monitor_init, glo_bias_timeout_health_monitor_deinit},
#ifdef ENABLE_SKYLARK_MONITOR
  {skylark_monitor_init, skylark_monitor_deinit},
#endif
  {ntrip_obs_timeout_health_monitor_init, ntrip_obs_timeout_health_monitor_deinit},
  {gnss_time_health_monitor_init, gnss_time_health_monitor_deinit},
  {base_num_sats_health_monitor_init, base_num_sats_health_monitor_deinit},
};
static size_t health_monitor_init_pairs_n =
  (sizeof(health_monitor_init_pairs) / sizeof(health_monitor_init_fn_pair_t));

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nOptions");
  puts("\t--debug   Enable debug log output");
}

static int parse_options(int argc, char *argv[])
{
  enum { OPT_ID_DEBUG = 1 };

  const struct option long_opts[] = {
    {"debug", no_argument, 0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_DEBUG: {
      health_ctx.health_debug = true;
    } break;
    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }

  return 0;
}

int main(int argc, char *argv[])
{
  int status = EXIT_SUCCESS;
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  piksi_log(LOG_DEBUG, "Startup...");

  health_ctx.loop = pk_loop_create();
  if (health_ctx.loop == NULL) {
    status = EXIT_FAILURE;
    goto cleanup;
  }

  health_ctx.sbp_ctx = sbp_pubsub_create(PROGRAM_NAME, SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (health_ctx.sbp_ctx == NULL) {
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(health_ctx.sbp_ctx), health_ctx.loop) != 0) {
    piksi_log(LOG_ERR, "Error registering for pubsub read!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  health_ctx.settings_ctx = pk_settings_create(SETTINGS_METRICS_NAME);
  if (health_ctx.settings_ctx == NULL) {
    piksi_log(LOG_ERR, "Error registering for settings!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (pk_settings_attach(health_ctx.settings_ctx, health_ctx.loop) != 0) {
    piksi_log(LOG_ERR, "Error registering for settings read!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  for (u8 i = 0; i < health_monitor_init_pairs_n; i++) {
    if (health_monitor_init_pairs[i].init(&health_ctx) != 0) {
      piksi_log(LOG_ERR, "Error setting up health monitor! id: %d", i);
    }
  }

  piksi_log(LOG_DEBUG, "Running...");
  pk_loop_run_simple(health_ctx.loop);

  for (u8 i = 0; i < health_monitor_init_pairs_n; i++) {
    health_monitor_init_pairs[i].deinit();
  }

cleanup:
  piksi_log(LOG_DEBUG, "Shutdown...");
  pk_loop_destroy(&health_ctx.loop);
  sbp_pubsub_destroy(&health_ctx.sbp_ctx);
  pk_settings_destroy(&health_ctx.settings_ctx);
  logging_deinit();

  exit(status);
}
