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

#include <float.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <czmq.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>

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

#define PROGRAM_NAME "health_daemon"

#define SBP_SUB_ENDPOINT ">tcp://127.0.0.1:43030" /* SBP External Out */
#define SBP_PUB_ENDPOINT ">tcp://127.0.0.1:43031" /* SBP External In */

struct health_ctx_s {
  double health_debug;
  sbp_zmq_pubsub_ctx_t *sbp_ctx;
};

bool health_context_get_debug(health_ctx_t *health_ctx)
{
  return health_ctx->health_debug;
}

sbp_zmq_pubsub_ctx_t *health_context_get_sbp_ctx(health_ctx_t *health_ctx)
{
  return health_ctx->sbp_ctx;
}

static health_ctx_t g_health_ctx = { .health_debug = false, .sbp_ctx = NULL };

static health_monitor_init_fn_pair_t health_monitor_init_pairs[] = {
  { baseline_threshold_health_monitor_init,
    baseline_threshold_health_monitor_deinit },
  { glo_obs_timeout_health_monitor_init,
    glo_obs_timeout_health_monitor_deinit },
  { glo_bias_timeout_health_monitor_init,
    glo_bias_timeout_health_monitor_deinit }
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
    { "debug", no_argument, 0, OPT_ID_DEBUG },
    { 0, 0, 0, 0 },
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_DEBUG: {
      g_health_ctx.health_debug = true;
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
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  piksi_log(LOG_DEBUG, "Startup...");

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  g_health_ctx.sbp_ctx =
    sbp_zmq_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (g_health_ctx.sbp_ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  for (u8 i = 0; i < health_monitor_init_pairs_n; i++) {
    if (health_monitor_init_pairs[i].init(&g_health_ctx) != 0) {
      piksi_log(LOG_ERR, "Error setting up health monitor! id: %d", i);
    }
  }

  piksi_log(LOG_DEBUG, "Running...");
  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(g_health_ctx.sbp_ctx));

  for (u8 i = 0; i < health_monitor_init_pairs_n; i++) {
    health_monitor_init_pairs[i].deinit();
  }

  sbp_zmq_pubsub_destroy(&g_health_ctx.sbp_ctx);
  piksi_log(LOG_DEBUG, "Shutdown...");
  logging_deinit();
  exit(EXIT_SUCCESS);
}
