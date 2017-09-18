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
#include <stdlib.h>
#include <unistd.h>
#include <libpiksi/logging.h>
#include <libnetwork.h>

#define PROGRAM_NAME "ntrip_daemon"

static bool debug = false;
static const char *fifo_file_path = NULL;
static const char *url = NULL;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMain options");
  puts("\t--file <file>");
  puts("\t--url <url>");

  puts("\nMisc options");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_FILE = 1,
    OPT_ID_URL,
    OPT_ID_DEBUG,
  };

  const struct option long_opts[] = {
    {"file",  required_argument, 0, OPT_ID_FILE},
    {"url  ", required_argument, 0, OPT_ID_URL},
    {"debug", no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case OPT_ID_FILE: {
        fifo_file_path = optarg;
      }
      break;

      case OPT_ID_URL: {
        url = optarg;
      }
      break;

      case OPT_ID_DEBUG: {
        debug = true;
      }
      break;

      default: {
        puts("Invalid option");
        return -1;
      }
      break;
    }
  }

  if (fifo_file_path == NULL) {
    puts("Missing file");
    return -1;
  }

  if (url == NULL) {
    puts("Missing url");
    return -1;
  }

  return 0;
}

static void terminate_handler(int signum)
{
  (void)signum;
  libnetwork_shutdown();  
}

static void cycle_connection(int signum)
{
  (void)signum;
  libnetwork_cycle_connection();
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  piksi_log(LOG_INFO, "Starting...");

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  int fd = open(fifo_file_path, O_WRONLY);
  if (fd < 0) {
    piksi_log(LOG_ERR, "fifo error (%d) \"%s\"", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  network_config_t config = {
    .url   = url,
    .fd    = fd,
    .debug = debug,
  };

  /* Set up handler for signals which should terminate the program */
  struct sigaction terminate_sa;
  terminate_sa.sa_handler = terminate_handler;
  sigemptyset(&terminate_sa.sa_mask);
  terminate_sa.sa_flags = 0;

  if ((sigaction(SIGINT, &terminate_sa, NULL) != 0) ||
      (sigaction(SIGTERM, &terminate_sa, NULL) != 0) ||
      (sigaction(SIGQUIT, &terminate_sa, NULL) != 0))
  {
    piksi_log(LOG_ERR, "error setting up terminate handler");
    exit(EXIT_FAILURE);
  }

  struct sigaction cycle_conn_sa;
  cycle_conn_sa.sa_handler = cycle_connection;
  sigemptyset(&cycle_conn_sa.sa_mask);
  cycle_conn_sa.sa_flags = 0;

  if ((sigaction(SIGUSR1, &cycle_conn_sa, NULL) != 0))
  {
    piksi_log(LOG_ERR, "error setting up SIGUSR1 handler");
    exit(EXIT_FAILURE);
  }

  ntrip_download(&config);

  piksi_log(LOG_INFO, "Shutting down");

  close(fd);
  logging_deinit();

  exit(EXIT_SUCCESS);
}
