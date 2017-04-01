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

#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <libskylark.h>

//
// Upload Daemon - connects to Skylark and sends SBP messages.
//

// Global configuration variables required for this daemon application.
static const char *named_source = NULL;
static const char *endpoint = NULL;
static bool verbose_logging = false;
static bool enabled = false;

static void usage(char *command)
{
  printf("Usage: %s\n", command);
  puts("\nPipe - Pipe to read data from");
  puts("\t-s, --sub <FIFO name>");
  puts("\nEndpoint - HTTP endpoint to upload to");
  puts("\t-e, --endpoint <HTTP endpoint>");
  puts("\t-v --verbose");
  puts("\t\tWrite status to stderr");
}

static int parse_options(int argc, char *argv[])
{
  const struct option long_opts[] = {{"sub", required_argument, 0, 's'},
                                     {"endpoint", required_argument, 0, 'e'},
                                     {"verbose", no_argument, 0, 'v'},
                                     {0, 0, 0, 0}};
  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "s:e:v", long_opts, &opt_index)) != -1) {
    switch (c) {
      case 's': {
        named_source = optarg;
        break;
      }
      case 'e': {
        endpoint = optarg;
        break;
      }
      case 'v': {
        verbose_logging = true;
        break;
      }
      default: {
        printf("invalid option\n");
        return -1;
      } break;
    }
  }
  if (named_source == NULL) {
    printf("Must specify the name of a pipe to read from.\n");
    return -1;
  }
  if (endpoint == NULL) {
    printf("Must specify an HTTP endpoint to connect to.\n");
    return -1;
  }
  return 0;
}

int main(int argc, char *argv[])
{
  log_error("starting upload daemon\n");
  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }
  int fd;
  if ((fd = open(named_source, O_RDONLY)) < 0) {
    printf("Error opening %s.\n", named_source);
    exit(EXIT_FAILURE);
  }
  RC rc = NO_ERROR;
  client_config_t config;
  if ((rc = init_config(&config)) < NO_ERROR) {
    log_client_error(rc);
    exit(EXIT_FAILURE);
  }
  config.fd = fd;
  strcpy(config.endpoint_url, endpoint);
  log_client_config(&config);
  if ((rc = setup_globals()) < NO_ERROR) {
    log_client_error(rc);
    exit(EXIT_FAILURE);
  }
  if ((rc = upload_process(&config, &upload_callback, verbose_logging)) <
      NO_ERROR) {
    log_client_error(rc);
    exit(EXIT_FAILURE);
  }
  log_debug("stopping upload daemon\n");
  teardown_globals();
  close(fd);
  exit(EXIT_FAILURE);
}
