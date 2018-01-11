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

static double gga_xfer_secs = 0.0;

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
    OPT_ID_INTERVAL,
  };

  const struct option long_opts[] = {
    {"file",      required_argument, 0, OPT_ID_FILE},
    {"url  ",     required_argument, 0, OPT_ID_URL},
    {"interval",  required_argument, 0, OPT_ID_INTERVAL},
    {"debug",     no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  char* endptr;
  int intarg;

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

      case OPT_ID_INTERVAL: {
          intarg = strtol(optarg, &endptr, 10);
          if (!(*optarg != '\0' && *endptr == '\0')) {
            printf("Invalid option\n");
            return -1;
          }
          gga_xfer_secs = intarg;
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

static bool configure_libnetwork(network_context_t* ctx, int fd) 
{
  network_status_t status = NETWORK_STATUS_SUCCESS;

  if ((status = libnetwork_set_url(ctx, url)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if ((status = libnetwork_set_fd(ctx, fd)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if ((status = libnetwork_set_debug(ctx, debug)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if ((status = libnetwork_set_gga_upload_interval(ctx, gga_xfer_secs))
      != NETWORK_STATUS_SUCCESS)
    goto exit_error;

  return true;

exit_error:
  piksi_log(LOG_ERR, "error configuring the libnetwork context: %d", status);
  return false;
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  piksi_log(LOG_INFO, "Starting...");

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  int print_gga_xfer_secs = (int) gga_xfer_secs;
  piksi_log(LOG_INFO, "GGA upload interval: %d seconds", print_gga_xfer_secs);

  int fd = open(fifo_file_path, O_WRONLY);
  if (fd < 0) {
    piksi_log(LOG_ERR, "fifo error (%d) \"%s\"", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  network_context_t* network_context = libnetwork_create(NETWORK_TYPE_NTRIP_DOWNLOAD);

  if(!configure_libnetwork(network_context, fd)) {
    exit(EXIT_FAILURE);
  }

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

  ntrip_download(network_context);

  piksi_log(LOG_INFO, "Shutting down");

  close(fd);
  logging_deinit();
  libnetwork_destroy(&network_context);

  exit(EXIT_SUCCESS);
}
