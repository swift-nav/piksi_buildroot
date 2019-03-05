/*
 * Copyright (C) 2016-2018 Swift Navigation Inc.
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

#define PROGRAM_NAME "sbp_fileio_daemon"

static const char *pub_endpoint = NULL;
static const char *sub_endpoint = NULL;
static path_validator_t *g_pv_ctx = NULL;

static bool allow_factory_mtd = false;
static bool allow_imageset_bin = false;
static bool print_usage = false;

static void usage(char *command);
static int parse_options(int argc, char *argv[]);

static void sigusr1_signal_cb(pk_loop_t *pk_loop, void *handle, int status, void *context);
static void terminate_handler(pk_loop_t *loop, void *handle, int status, void *context);
static bool setup_terminate_handler(pk_loop_t *pk_loop);

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("-h, --help            Print this message");
  puts("-n, --name    <name>  The name of this fileio daemon");
  puts("-p, --pub     <addr>  The address on which we should write the results of");
  puts("                      MSG_FILEIO_* messages");
  puts("-s, --sub     <addr>  The address on which we should listen for MSG_FILEIO_*");
  puts("                      messages");
  puts("-b, --basedir <path>  The base directory that should prefix all read, write,");
  puts("                      remove and list operations.");
  puts("-m, --mtd             Allow read access to /factory/mtd");
  puts("-i, --imageset        Allow write access to upgrade.image_set.bin, internally");
  puts("                      the file will be written to /data/upgrade.image_set.bin");
  puts("-d, --debug           Output debug logging");
  puts("-x, --nocache         Disable FD cache");
  puts("-t, --no-threading    Disable threading");
}

static int parse_options(int argc, char *argv[])
{
  // Used in --basedir option processing
  assert(g_pv_ctx != NULL);

  // clang-format off
  const struct option long_opts[] = {
    {"name",         required_argument, 0, 'n'},
    {"pub",          required_argument, 0, 'p'},
    {"sub",          required_argument, 0, 's'},
    {"basedir",      required_argument, 0, 'b'},
    {"mtd",          no_argument,       0, 'm'},
    {"imageset",     no_argument,       0, 'i'},
    {"debug",        no_argument,       0, 'd'},
    {"nocache",      no_argument,       0, 'x'},
    {"help",         no_argument,       0, 'h'},
    {"no-threading", no_argument,       0, 't'},
    {0, 0, 0, 0}
  };
  // clang-format on

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "n:p:s:b:midxht", long_opts, &opt_index)) != -1) {
    switch (c) {

    case 'n': {
      sbp_fileio_name = optarg;
    } break;

    case 'p': {
      pub_endpoint = optarg;
    } break;

    case 's': {
      sub_endpoint = optarg;
    } break;

    case 'b': {
      if (!path_validator_allow_path(g_pv_ctx, optarg)) {
        fprintf(stderr, "Error: failed to allow path with --basedir\n");
        return -1;
      }
    } break;

    case 'm': {
      allow_factory_mtd = true;
    } break;

    case 'i': {
      allow_imageset_bin = true;
    } break;

    case 'h': {
      print_usage = true;
    } break;

    case 'd': {
      fio_debug = true;
    } break;

    case 'x': {
      no_cache = true;
    } break;

    case 't': {
      disable_threading = true;
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

  if ((pub_endpoint == NULL) || (sub_endpoint == NULL)) {
    fprintf(stderr, "Endpoints not specified\n");
    return -1;
  }

  if (path_validator_allowed_count(g_pv_ctx) == 0) {
    fprintf(stderr, "Base directory path(s) must be specified and non-empty.\n");
    return -1;
  }

  return 0;
}

static void sigusr1_signal_cb(pk_loop_t *pk_loop, void *handle, int status, void *context)
{
  (void)pk_loop;
  (void)handle;
  (void)status;
  (void)context;

  piksi_log(LOG_DEBUG, "Received SIGUSR1 signal! Flushing buffers...");
  sbp_fileio_flush();
}

static void terminate_handler(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)context;
  (void)status;

  int signum = pk_loop_get_signal_from_handle(handle);
  piksi_log(LOG_DEBUG, "received signal: %d", signum);

  pk_loop_stop(loop);
}

static bool setup_terminate_handler(pk_loop_t *pk_loop)
{
  if (pk_loop_signal_handler_add(pk_loop, SIGINT, terminate_handler, NULL) == NULL) {
    piksi_log(LOG_ERR, "Failed to add SIGINT handler to loop");
    return false;
  }
  if (pk_loop_signal_handler_add(pk_loop, SIGTERM, terminate_handler, NULL) == NULL) {
    piksi_log(LOG_ERR, "Failed to add SIGTERM handler to loop");
    return false;
  }
  if (pk_loop_signal_handler_add(pk_loop, SIGQUIT, terminate_handler, NULL) == NULL) {
    piksi_log(LOG_ERR, "Failed to add SIGTERM handler to loop");
    return false;
  }
  return true;
}

int main(int argc, char *argv[])
{
  bool setup = false;
  pk_loop_t *loop = NULL;
  sbp_pubsub_ctx_t *ctx = NULL;
  char identity[128] = {0};

  int ret = EXIT_FAILURE;
  logging_init(PROGRAM_NAME);

  g_pv_ctx = path_validator_create(NULL);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    goto cleanup;
  }

  if (print_usage) {
    usage(argv[0]);
    goto cleanup;
  }

  path_validator_setup_metrics(g_pv_ctx, sbp_fileio_name);
  snprintf_assert(identity, sizeof(identity), "%s/%s", PROGRAM_NAME, sbp_fileio_name);

  ctx = sbp_pubsub_create(identity, pub_endpoint, sub_endpoint);
  if (ctx == NULL) {
    goto cleanup;
  }

  loop = pk_loop_create();
  if (loop == NULL) {
    goto cleanup;
  }

  if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(ctx), loop) != 0) {
    goto cleanup;
  }

  if (pk_loop_signal_handler_add(loop, SIGUSR1, sigusr1_signal_cb, NULL) == NULL) {
    piksi_log(LOG_ERR, "Error registering signal handler!");
    goto cleanup;
  }

  if (!setup_terminate_handler(loop)) {
    goto cleanup;
  }

  setup = sbp_fileio_setup(sbp_fileio_name,
                           loop,
                           g_pv_ctx,
                           allow_factory_mtd,
                           allow_imageset_bin,
                           sbp_pubsub_rx_ctx_get(ctx),
                           sbp_pubsub_tx_ctx_get(ctx));

  if (!setup) {
    goto cleanup;
  }

  piksi_log(LOG_INFO, "loop starting...");

  pk_loop_run_simple(loop);
  ret = EXIT_SUCCESS;

cleanup:
  piksi_log(LOG_INFO, "loop stopping...");

  sbp_pubsub_destroy(&ctx);
  pk_loop_destroy(&loop);

  path_validator_destroy(&g_pv_ctx);
  sbp_fileio_teardown(sbp_fileio_name);

  logging_deinit();

  exit(ret);
}
