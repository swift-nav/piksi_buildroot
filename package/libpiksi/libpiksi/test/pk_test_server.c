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
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include <libpiksi/endpoint.h>
#include <libpiksi/loop.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#define PROGRAM_NAME "pk_test_server"

#define MI metrics_indexes
#define MT message_metrics_table
#define MR router_metrics

static pk_endpoint_t *pub_ept = NULL;
static pk_endpoint_t *sub_ept = NULL;

static struct {
  const char *pub;
  const char *sub;
} options = {
  .pub = NULL,
  .sub = NULL,
};

static void loop_reader_callback(pk_loop_t *loop, void *handle, int status, void *context);

static void usage(char *command)
{
  printf("Usage: %s\n", command);
  puts("--pub <ipc:///path/to/pub_port_name>");
  puts("--sub <ipc:///path/to/sub_port_name>");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_PUB = 1,
    OPT_ID_SUB,
  };

  // clang-format off
  const struct option long_opts[] = {
    {"pub",      required_argument, 0, OPT_ID_PUB},
    {"sub",      required_argument, 0, OPT_ID_SUB},
    {0, 0, 0, 0},
  };
  // clang-format on

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "", long_opts, &opt_index)) != -1) {
    switch (c) {

    case OPT_ID_PUB: {
      options.pub = optarg;
    } break;

    case OPT_ID_SUB: {
      options.sub = optarg;
    } break;

    default: {
      printf("invalid option\n");
      return -1;
    } break;
    }
  }
  if (options.pub == NULL) {
    printf("server pub port name not specified\n");
    return -1;
  }

  if (options.sub == NULL) {
    printf("server sub port name not specified\n");
    return -1;
  }

  return 0;
}

static int reader_fn(const u8 *data, const size_t length, void *context)
{
  (void)data;
  (void)length;
  (void)context;

  pk_endpoint_send(pub_ept, data, length);
  PK_LOG_ANNO(LOG_DEBUG, "length: %d", length);

  return 0;
}

static void loop_reader_callback(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)handle;
  (void)status;
  (void)context;

  pk_endpoint_receive(sub_ept, reader_fn, NULL);
}

static int cleanup(int result, pk_loop_t **loop_loc);

int main(int argc, char *argv[])
{
  int rc = 0;
  pk_loop_t *loop = NULL;

  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(cleanup(EXIT_FAILURE, &loop));
  }

  /* Create loop */
  loop = pk_loop_create();
  if (loop == NULL) {
    PK_LOG_ANNO(LOG_ERR, "failed to create event loop");
    exit(cleanup(EXIT_FAILURE, &loop));
  }

  pub_ept = pk_endpoint_create(pk_endpoint_config()
                                 .endpoint(options.pub)
                                 .identity("pub_test_svr")
                                 .type(PK_ENDPOINT_PUB_SERVER)
                                 .get());
  if (pub_ept == NULL) {
    PK_LOG_ANNO(LOG_ERR, "failed to create pub endpoint");
    exit(cleanup(EXIT_FAILURE, &loop));
  }

  sub_ept = pk_endpoint_create(pk_endpoint_config()
                                 .endpoint(options.sub)
                                 .identity("sub_test_svr")
                                 .type(PK_ENDPOINT_SUB_SERVER)
                                 .get());
  if (pub_ept == NULL) {
    PK_LOG_ANNO(LOG_ERR, "failed to create sub endpoint");
    exit(cleanup(EXIT_FAILURE, &loop));
  }

  rc = pk_endpoint_loop_add(pub_ept, loop);
  if (rc < 0) {
    PK_LOG_ANNO(LOG_ERR, "failed to add endpoint to loop");
    exit(cleanup(EXIT_FAILURE, &loop));
  }

  rc = pk_endpoint_loop_add(sub_ept, loop);
  if (rc < 0) {
    PK_LOG_ANNO(LOG_ERR, "failed to add endpoint to loop");
    exit(cleanup(EXIT_FAILURE, &loop));
  }

  if (pk_loop_endpoint_reader_add(loop, sub_ept, loop_reader_callback, NULL) == NULL) {
    PK_LOG_ANNO(LOG_ERR, "pk_loop_endpoint_reader_add() error");
    exit(cleanup(EXIT_FAILURE, &loop));
  }

  pk_loop_run_simple(loop);

  exit(cleanup(EXIT_SUCCESS, &loop));
}

static int cleanup(int result, pk_loop_t **loop_loc)
{
  pk_endpoint_destroy(&pub_ept);
  pk_endpoint_destroy(&sub_ept);

  pk_loop_destroy(loop_loc);

  logging_deinit();

  return result;
}
