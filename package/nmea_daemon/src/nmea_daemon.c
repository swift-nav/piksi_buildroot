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

#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <libpiksi/logging.h>
#include <libpiksi/endpoint.h>
#include <libpiksi/loop.h>

#define PROGRAM_NAME      "nmea_daemon"

#define NMEA_PUB_ENDPOINT "tcp://127.0.0.1:44030"  /* NMEA Pub */

#define BASE_DIRECTORY    "/var/run/nmea"

const char* const NMEA_GGA_OUTPUT_PATH = BASE_DIRECTORY "/GGA";

bool nmea_debug = false;

typedef struct nmea_ctx_s {
  pk_endpoint_t *sub_ept;
  pk_loop_t *loop;
  int result;
} nmea_ctx_t;

static int handle_frame_cb(const u8 *frame_data, const size_t frame_length, void *context);

#define MAX_BUFFER 2048

char buffer[MAX_BUFFER];

static void nmea_reader_handler(pk_loop_t *loop, void *handle, void *context)
{
  (void)loop;
  (void)handle;
  nmea_ctx_t *ctx = (nmea_ctx_t *)context;

  if (pk_endpoint_receive(ctx->sub_ept, handle_frame_cb, ctx) != 0) {
    piksi_log(LOG_ERR, "endpoint receive failed, stopping loop...");
    ctx->result = EXIT_FAILURE;
  }
  if (ctx->result == EXIT_FAILURE) {
    piksi_log(LOG_ERR, "nmea frame handler failed, stopping loop...");
    pk_loop_stop(loop);
  }
}

static int handle_frame_cb(const u8 *frame_data, const size_t frame_length, void *context)
{
  nmea_ctx_t *ctx = (nmea_ctx_t *)context;

  if (ctx->result == EXIT_FAILURE) {
    sbp_log(LOG_ERR, "previous frame result was failure, skipping frame");
    return -1;
  }

  {
    // Bounds check: MAX_BUFFER-1 so we can always null terminate the string
    if (frame_length == 0 || frame_length > (MAX_BUFFER-1)) {
      sbp_log(LOG_ERR, "invalid frame size: %lu", frame_length);
      ctx->result = EXIT_FAILURE;
      return -1;
    }

    memcpy(buffer, frame_data, MAX_BUFFER-1);
    buffer[frame_length] = '\0';
  }

  if(strstr(buffer, "GGA") == NULL) {
    if (nmea_debug) {
      piksi_log(LOG_DEBUG, "ignoring non-GGA message");
    }
    return 0;
  }

  if (nmea_debug) {
    piksi_log(LOG_DEBUG, "got GGA message: %s", buffer);
  }

  char tmp_file_name[] = BASE_DIRECTORY "/temp_nmea_gga.XXXXXX";

  int fd_temp = mkstemp(tmp_file_name);
  fchmod(fd_temp, 0644);

  if (fd_temp < 0) {
    sbp_log(LOG_ERR, "error creating temp file: %s", strerror(errno));
    ctx->result = EXIT_FAILURE;
    return -1;
  }

  FILE* fp = fdopen(fd_temp, "w+");

  if (strstr(buffer, "\r\n") != NULL) {
    fprintf(fp, "%s", buffer);
  } else {
    fprintf(fp, "%s\r\n", buffer);
  }

  fclose(fp);

  if (rename(tmp_file_name, NMEA_GGA_OUTPUT_PATH) < 0) {
    sbp_log(LOG_WARNING, "rename failed: %s", strerror(errno));
    return 0;
  }

  return 0;
}

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nOptions:");
  puts("\t--debug   Enable debug messages");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_DEBUG = 1,
  };

  const struct option long_opts[] = {
    {"debug", no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case OPT_ID_DEBUG:
        nmea_debug = true;
        break;
      default:
        puts("Invalid option");
        return -1;
    }
  }
  return 0;
}

static int cleanup(int result, nmea_ctx_t *ctx);

int main(int argc, char *argv[])
{
  nmea_ctx_t ctx = { .sub_ept = NULL, .loop = NULL , .result = EXIT_SUCCESS };

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(cleanup(EXIT_FAILURE, &ctx));
  }

  ctx.sub_ept = pk_endpoint_create(NMEA_PUB_ENDPOINT, PK_ENDPOINT_SUB);
  if (ctx.sub_ept == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(cleanup(EXIT_FAILURE, &ctx));
  }

  ctx.loop = pk_loop_create();
  if (ctx.loop == NULL) {
    piksi_log(LOG_ERR, "error creating loop");
    exit(cleanup(EXIT_FAILURE, &ctx));
  }

  if (pk_loop_endpoint_reader_add(ctx.loop,
                                  ctx.sub_ept,
                                  nmea_reader_handler,
                                  &ctx) == NULL) {
    piksi_log(LOG_ERR, "error registering reader");
    exit(cleanup(EXIT_FAILURE, &ctx));
  }

  pk_loop_run_simple(ctx.loop);

  exit(cleanup(ctx.result, &ctx));
}

static int cleanup(int result, nmea_ctx_t *ctx)
{
  pk_loop_destroy(&ctx->loop);
  pk_endpoint_destroy(&ctx->sub_ept);
  return result;
}
