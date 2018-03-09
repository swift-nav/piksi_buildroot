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

#include <czmq.h>

#include <libpiksi/util.h>
#include <libpiksi/logging.h>

#define PROGRAM_NAME      "nmea_daemon"

#define NMEA_PUB_ENDPOINT ">tcp://127.0.0.1:44030"  /* NMEA Pub */

#define BASE_DIRECTORY    "/var/run/nmea"

const char*const NMEA_GGA_OUTPUT_PATH = BASE_DIRECTORY "/GGA";

bool nmea_debug = false;

#define MAX_BUFFER 2048

char buffer[MAX_BUFFER];

static int nmea_reader_handler(zloop_t *zloop, zsock_t *zsock, void *arg)
{
  (void)zloop;
  (void)arg;

  zmsg_t *msg;

  while (1) {
    msg = zmsg_recv(zsock);
    if (msg != NULL) {
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      sbp_log(LOG_ERR, "error in zmsg_recv()");
      exit(EXIT_FAILURE);
    }
  }

  size_t frame_count = zmsg_size(msg);

  if (frame_count != 1) {
    sbp_log(LOG_ERR, "zmsg frame count was invalid");
    exit(EXIT_FAILURE);
  }

  zframe_t *frame = zmsg_first(msg);
  size_t frame_length = 0;

  {
    const char *frame_data = (char*) zframe_data(frame);
    frame_length = zframe_size(frame);

    // Bounds check: MAX_BUFFER-1 so we can always null terminate the string
    if (frame_length == 0 || frame_length > (MAX_BUFFER-1)) {
      sbp_log(LOG_ERR, "invalid frame size: %lu", frame_length);
      exit(EXIT_FAILURE);
    }

    memcpy(buffer, frame_data, MAX_BUFFER-1);
    buffer[frame_length] = '\0';
  }

  if(strstr(buffer, "GGA") == NULL) {
    if (nmea_debug) {
      piksi_log(LOG_DEBUG, "ignoring non-GGA message");
    }
    goto exit_success;
  }

  if (nmea_debug) {
    piksi_log(LOG_DEBUG, "got GGA message: %s", buffer);
  }

  char tmp_file_name[] = BASE_DIRECTORY "/temp_nmea_gga.XXXXXX";

  int fd_temp = mkstemp(tmp_file_name);
  fchmod(fd_temp, 0644);

  if (fd_temp < 0) {
    sbp_log(LOG_ERR, "error creating temp file: %s", strerror(errno));
    exit(EXIT_FAILURE);
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

exit_success:
  zmsg_destroy(&msg);
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

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  zsock_t * zmq_sub = zsock_new_sub(NMEA_PUB_ENDPOINT, "");
  if (zmq_sub == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(EXIT_FAILURE);
  }

  zloop_t* loop = zloop_new();

  if (loop == NULL) {
    piksi_log(LOG_ERR, "error creating looper");
    exit(EXIT_FAILURE);
  }

  if(zloop_reader(loop, zmq_sub, nmea_reader_handler, NULL) < 0) {
    piksi_log(LOG_ERR, "error registering reader");
    exit(EXIT_FAILURE);
  }

  if(zmq_simple_loop(loop) < 0) {
    exit(EXIT_FAILURE);
  }

  zsock_destroy(&zmq_sub);
  exit(EXIT_SUCCESS);
}
