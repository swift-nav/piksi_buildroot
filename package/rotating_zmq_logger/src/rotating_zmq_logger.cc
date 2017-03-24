/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jonathan Diamond <jonathan@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <assert.h>
#include <czmq.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rotating_logger.h"

#define SLICE_DURATION_DEFAULT_m 10
#define POLL_PERIOD_DEFAULT_s 30
#define FILL_THRESHOLD_DEFAULT_p 95

static int slice_diration_m = SLICE_DURATION_DEFAULT_m;
static int poll_period_s = POLL_PERIOD_DEFAULT_s;
static int fill_threshold_p = FILL_THRESHOLD_DEFAULT_p;

static const char *zmq_sub_endpoint = nullptr;
static const char *dir_path = nullptr;

static bool verbose_logging = false;

static void usage(char *command) {
  printf("Usage: %s\n", command);

  puts("\nSource - zmq sub endpoint");
  puts("\t-s, --sub <addr>");

  puts("\nSink Directory - directory to write logs");
  puts("\t-d, --dir <file>");

  puts("\nMisc options - optional");
  puts("\t--slice-duration <minutes>");
  puts("\t\tduration of files before rolling over (default: 10)");
  puts("\t--poll-period <seconds>");
  puts("\t\tPeriod between checking sink dir available (default: 30)");
  puts("\t--full-threshold <precent>");
  puts(
      "\t\tStop logging if disk is filled above this percentage (default: 95)");
  puts("\t-v --verbose");
  puts("\t\tWrite status to stdout");
}

static int parse_options(int argc, char *argv[]) {
  enum { OPT_ID_DURATION = 1, OPT_ID_PERIOD, OPT_ID_THRESHOLD, OPT_ID_FLUSH };

  const struct option long_opts[] = {
      {"sub", required_argument, nullptr, 's'},
      {"dir", required_argument, nullptr, 'd'},
      {"verbose", no_argument, nullptr, 'v'},
      {"slice-duration", required_argument, nullptr, OPT_ID_DURATION},
      {"poll-period", required_argument, nullptr, OPT_ID_PERIOD},
      {"full-threshold", required_argument, nullptr, OPT_ID_THRESHOLD},
      {nullptr, 0, nullptr, 0}};

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "s:d:v", long_opts, &opt_index)) != -1) {
    switch (c) {
      case 's': {
        zmq_sub_endpoint = optarg;
      } break;

      case 'd': {
        dir_path = optarg;
      } break;

      case OPT_ID_DURATION: {
        slice_diration_m = strtol(optarg, nullptr, 10);
      } break;

      case OPT_ID_PERIOD: {
        poll_period_s = strtol(optarg, nullptr, 10);
      } break;

      case OPT_ID_THRESHOLD: {
        fill_threshold_p = strtol(optarg, nullptr, 10);
      } break;

      case 'v': {
        verbose_logging = true;
      } break;

      default: {
        printf("invalid option\n");
        return -1;
      } break;
    }
  }

  if (zmq_sub_endpoint == nullptr) {
    printf("Must specify source\n");
    return -1;
  }

  if (dir_path == nullptr) {
    printf("Must specify sink directory\n");
    return -1;
  }

  return 0;
}

static void sigchld_handler(int signum) {
  (void)signum;
  int saved_errno = errno;
  while (waitpid(-1, nullptr, WNOHANG) > 0) {
    ;
  }
  errno = saved_errno;
}

static void terminate_handler(int signum) {
  /* Send this signal to the entire process group */
  killpg(0, signum);

  /* Exit */
  _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  setpgid(0, 0); /* Set PGID = PID */

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(1);
  }

  /* Prevent czmq from catching signals */
  zsys_handler_set(nullptr);

  signal(SIGPIPE, SIG_IGN); /* Allow write to return an error */

  /* Set up SIGCHLD handler */
  struct sigaction sigchld_sa;
  sigchld_sa.sa_handler = sigchld_handler;
  sigemptyset(&sigchld_sa.sa_mask);
  sigchld_sa.sa_flags = SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sigchld_sa, nullptr) != 0) {
    printf("error setting up sigchld handler\n");
    exit(EXIT_FAILURE);
  }

  /* Set up handler for signals which should terminate the program */
  struct sigaction terminate_sa;
  terminate_sa.sa_handler = terminate_handler;
  sigemptyset(&terminate_sa.sa_mask);
  terminate_sa.sa_flags = 0;
  if ((sigaction(SIGINT, &terminate_sa, nullptr) != 0) ||
      (sigaction(SIGTERM, &terminate_sa, nullptr) != 0) ||
      (sigaction(SIGQUIT, &terminate_sa, nullptr) != 0)) {
    printf("error setting up terminate handler\n");
    exit(EXIT_FAILURE);
  }

  RotatingLogger logger(dir_path, slice_diration_m, poll_period_s,
                        fill_threshold_p, verbose_logging);

  zsock_t *zmq_sub = zsock_new_sub(zmq_sub_endpoint, "");
  if (zmq_sub == nullptr) {
    printf("error creating SUB socket\n");
    exit(EXIT_FAILURE);
  }

  while (true) {
    zmsg_t *msg = zmsg_recv(zmq_sub);
    if (msg == nullptr) {
      continue;
    }

    zframe_t *frame;
    for (frame = zmsg_first(msg); frame != nullptr; frame = zmsg_next(msg)) {
      logger.frame_handler(zframe_data(frame), zframe_size(frame));
    }

    zmsg_destroy(&msg);
  }

  exit(EXIT_SUCCESS);
}
