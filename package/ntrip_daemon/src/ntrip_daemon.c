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
#include <errno.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>

#include <libnetwork.h>

#include "ntrip_settings.h"

#define NTRIP_CONTROL_FILE "/var/run/ntrip/control/socket"
#define NTRIP_CONTROL_SOCK "ipc://" NTRIP_CONTROL_FILE

#define NTRIP_CONTROL_COMMAND_RECONNECT "r"

#define PROGRAM_NAME "ntrip_daemon"

static bool debug = false;
static const char *fifo_file_path = NULL;
static const char *username = NULL;
static const char *password = NULL;
static const char *url = NULL;
static bool rev1gga = false;

static double gga_xfer_secs = 0.0;

typedef enum {
  OP_MODE_NTRIP_CLIENT,
  OP_MODE_SETTINGS_DAEMON,
  OP_MODE_REQ_RECONNECT,
} operating_mode;

static operating_mode op_mode = OP_MODE_NTRIP_CLIENT;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMode selection options");
  puts("\t--ntrip      launch in ntrip client mode");
  puts("\t--settings   launch in settings monitor mode");
  puts("\t--reconnect  request that the NTRIP client reconnect");

  puts("\nNTRIP mode options");
  puts("\t--file       <file>");
  puts("\t--username   <username>");
  puts("\t--password   <password>");
  puts("\t--url        <url>");

  puts("\nGGA options");
  puts("\t--interval <secs>");
  puts("\t--rev1gga <y|n>");

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
    OPT_ID_REV1GGA,
    OPT_ID_USERNAME,
    OPT_ID_PASSWORD,
    OPT_ID_MODE_NTRIP,
    OPT_ID_MODE_SETTINGS,
    OPT_ID_MODE_RECONNECT,
  };

  const struct option long_opts[] = {
    {"ntrip",     no_argument,       0, OPT_ID_MODE_NTRIP},
    {"settings",  no_argument,       0, OPT_ID_MODE_SETTINGS},
    {"reconnect", no_argument,       0, OPT_ID_MODE_RECONNECT},
    {"file",      required_argument, 0, OPT_ID_FILE},
    {"username",  required_argument, 0, OPT_ID_USERNAME},
    {"password",  required_argument, 0, OPT_ID_PASSWORD},
    {"url  ",     required_argument, 0, OPT_ID_URL},
    {"interval",  required_argument, 0, OPT_ID_INTERVAL},
    {"rev1gga",   required_argument, 0, OPT_ID_REV1GGA},
    {"debug",     no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  char* endptr;
  int intarg;

  int opt;

  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {

      case OPT_ID_MODE_NTRIP: {
        op_mode = OP_MODE_NTRIP_CLIENT;
      }
      break;

      case OPT_ID_MODE_SETTINGS: {
        op_mode = OP_MODE_SETTINGS_DAEMON;
      }
      break;

      case OPT_ID_MODE_RECONNECT: {
        op_mode = OP_MODE_REQ_RECONNECT;
      }
      break;

      case OPT_ID_FILE: {
        fifo_file_path = optarg;
      }
      break;

      case OPT_ID_USERNAME: {
        username = optarg;
      }
      break;

      case OPT_ID_PASSWORD: {
        password = optarg;
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

      case OPT_ID_REV1GGA: {
        rev1gga = *optarg == 'y';
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

  if (op_mode == OP_MODE_NTRIP_CLIENT && fifo_file_path == NULL) {
    puts("Missing file");
    return -1;
  }

  if (op_mode == OP_MODE_NTRIP_CLIENT && url == NULL) {
    puts("Missing url");
    return -1;
  }

  return 0;
}

static void network_terminate_handler(int signum, siginfo_t *info, void *ucontext)
{
  (void)ucontext;
  piksi_log(LOG_DEBUG, "%s: received signal: %d, sender: %d",
            __FUNCTION__, signum, info->si_pid);
  libnetwork_shutdown();
}

static void cycle_connection(int signum)
{
  piksi_log(LOG_DEBUG, "%s: received signal: %d", __FUNCTION__, signum);
  libnetwork_cycle_connection();
}

static bool configure_libnetwork(network_context_t* ctx, int fd)
{
  network_status_t status = NETWORK_STATUS_SUCCESS;

  if (username != NULL) {
    if ((status = libnetwork_set_username(ctx, username)) != NETWORK_STATUS_SUCCESS)
      goto exit_error;
  }
  if (password != NULL) {
    if ((status = libnetwork_set_password(ctx, password)) != NETWORK_STATUS_SUCCESS)
      goto exit_error;
  }
  if ((status = libnetwork_set_url(ctx, url)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if ((status = libnetwork_set_fd(ctx, fd)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if ((status = libnetwork_set_debug(ctx, debug)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if ((status = libnetwork_set_gga_upload_interval(ctx, gga_xfer_secs))
      != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if ((status = libnetwork_set_gga_upload_rev1(ctx, rev1gga)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;

  return true;

exit_error:
  piksi_log(LOG_ERR, "error configuring the libnetwork context: %d", status);
  return false;
}

static int ntrip_client_loop(void)
{
  piksi_log(LOG_INFO, "Starting NTRIP client connection...");

  int print_gga_xfer_secs = (int) gga_xfer_secs;
  piksi_log(LOG_INFO, "GGA upload interval: %d seconds", print_gga_xfer_secs);

  int fd = open(fifo_file_path, O_WRONLY);
  if (fd < 0) {
    piksi_log(LOG_ERR, "fifo error (%d) \"%s\"", errno, strerror(errno));
    return -1;
  }

  network_context_t* network_context = libnetwork_create(NETWORK_TYPE_NTRIP_DOWNLOAD);

  if(!configure_libnetwork(network_context, fd)) {
    return -1;
  }

  setup_sigint_handler(network_terminate_handler);
  setup_sigterm_handler(network_terminate_handler);

  struct sigaction cycle_conn_sa;
  cycle_conn_sa.sa_handler = cycle_connection;
  sigemptyset(&cycle_conn_sa.sa_mask);
  cycle_conn_sa.sa_flags = 0;

  if ((sigaction(SIGUSR1, &cycle_conn_sa, NULL) != 0))
  {
    piksi_log(LOG_ERR, "error setting up SIGUSR1 handler");
    return -1;
  }

  ntrip_download(network_context);

  piksi_log(LOG_INFO, "Shutting down");

  close(fd);
  libnetwork_destroy(&network_context);

  return 0;
}

static void settings_loop_sigchild()
{
  reap_children(debug, ntrip_record_exit);
}

static void settings_loop_terminate(void)
{
  ntrip_stop_processes();
  reap_children(debug, NULL);
}

static int ntrip_settings_loop(void)
{
  return settings_loop(NTRIP_CONTROL_SOCK,
                       NTRIP_CONTROL_FILE,
                       NTRIP_CONTROL_COMMAND_RECONNECT,
                       ntrip_settings_init,
                       ntrip_reconnect,
                       settings_loop_terminate,
                       settings_loop_sigchild) ? 0 : -1;
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  int ret = 0;

  switch(op_mode) {
  case OP_MODE_NTRIP_CLIENT:
    ret = ntrip_client_loop();
    break;
  case OP_MODE_SETTINGS_DAEMON:
    ret = ntrip_settings_loop();
    break;
  case OP_MODE_REQ_RECONNECT:
    ret = settings_loop_send_command("NTRIP client",
                                     NTRIP_CONTROL_COMMAND_RECONNECT,
                                     "reconnect",
                                     NTRIP_CONTROL_SOCK);
    break;
  default:
    assert(false);
  }

  logging_deinit();

  if (ret != 0)
    exit(EXIT_FAILURE);

  exit(EXIT_SUCCESS);
}
