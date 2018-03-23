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

#include "skylark_settings.h"

#define PROGRAM_NAME "skylark_daemon"

#define SKYLARK_CONTROL_FILE "/var/run/skylark/control/socket"
#define SKYLARK_CONTROL_SOCK "ipc://" NTRIP_CONTROL_FILE

#define SKYLARK_CONTROL_COMMAND_RECONNECT "r"

static bool debug = false;
static const char *fifo_file_path = NULL;
static const char *url = NULL;

typedef enum {
  OP_MODE_NONE,
  OP_MODE_DOWNLOAD,
  OP_MODE_UPLOAD,
  OP_MODE_SETTINGS,
} operating_mode;

static operating_mode op_mode = OP_MODE_NONE;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMode selection options");
  puts("\t--upload         Launch in skylark upload mode");
  puts("\t--download       Launch in skylark download mode");
  puts("\t--settings       Launch in settings daemon mode");
  puts("\t--reconnect-dl   Ask the download daemon to reconnect");

  puts("\nUpload/download mode options");
  puts("\t--file           <file>");
  puts("\t--url            <url>");

  puts("\nMisc options");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_FILE = 1,
    OPT_ID_URL,
    OPT_ID_DEBUG,
    OPT_ID_UPLOAD,
    OPT_ID_DOWNLOAD,
    OPT_ID_SETTINGS,
  };

  const struct option long_opts[] = {
    {"upload",   required_argument, 0, OPT_ID_UPLOAD},
    {"download", required_argument, 0, OPT_ID_DOWNLOAD},
    {"settings", required_argument, 0, OPT_ID_SETTINGS},
    {"file",     required_argument, 0, OPT_ID_FILE},
    {"url  ",    required_argument, 0, OPT_ID_URL},
    {"debug",    no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {

      case OPT_ID_FILE: {
        fifo_file_path = optarg;
      }
      break;

      case OPT_ID_UPLOAD: {
        op_mode = OP_MODE_UPLOAD;
      }
      break;

      case OPT_ID_DOWNLOAD: {
        op_mode = OP_MODE_UPLOAD;
      }
      break;

      case OPT_ID_SETTINGS: {
        op_mode = OP_MODE_UPLOAD;
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

  if (op_mode == OP_MODE_DOWNLOAD && fifo_file_path == NULL) {
    puts("Missing file");
    return -1;
  }

  if (op_mode == OP_MODE_DOWNLOAD && url == NULL) {
    puts("Missing url");
    return -1;
  }

  return 0;
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

  return true;

exit_error:
  piksi_log(LOG_ERR, "error configuring the libnetwork context: %d", status);
  return false;
}

int skylark_upload_mode()
{
  int fd = open(fifo_file_path, O_RDONLY);
  if (fd < 0) {
    piksi_log(LOG_ERR, "fifo error (%d) \"%s\"", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  network_context_t* network_context = libnetwork_create(NETWORK_TYPE_SKYLARK_UPLOAD);

  if(!configure_libnetwork(network_context, fd)) {
    exit(EXIT_FAILURE);
  }

  skylark_upload(network_context);

  close(fd);
  libnetwork_destroy(&network_context);
}

static void skylark_download_mode()
{
  int fd = open(fifo_file_path, O_WRONLY);
  if (fd < 0) {
    piksi_log(LOG_ERR, "fifo error (%d) \"%s\"", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  struct sigaction cycle_conn_sa;
  cycle_conn_sa.sa_handler = cycle_connection;
  sigemptyset(&cycle_conn_sa.sa_mask);
  cycle_conn_sa.sa_flags = 0;

  if ((sigaction(SIGUSR1, &cycle_conn_sa, NULL) != 0)) {
    piksi_log(LOG_ERR, "error setting up SIGUSR1 handler");
    exit(EXIT_FAILURE);
  }

  network_context_t* network_context = libnetwork_create(NETWORK_TYPE_SKYLARK_DOWNLOAD);

  if(!configure_libnetwork(network_context, fd)) {
    exit(EXIT_FAILURE);
  }

  skylark_download(network_context);

  close(fd);
  libnetwork_destroy(&network_context);
}

static void skylark_settings_loop(void)
{
  settings_loop(skylark_init,
                SKYLARK_CONTROL_SOCK,
                SKYLARK_CONTROL_FILE,
                SKYLARK_CONTROL_COMMAND_RECONNECT,
                skylark_reconnect_dl());
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  switch(op_mode) {
  case OP_MODE_DOWNLOAD:
    skylark_download_mode();
    break;
  case OP_MODE_SETTINGS:
    skylark_settings_loop();
    break;
  case OP_MODE_UPLOAD:
    skylark_upload_mode();
    break;
  case OP_MODE_NONE:
  default:
    piksi_log(LOG_ERR, "unknown operating mode");
    exit(EXIT_FAILURE);
  }

  logging_deinit();
  exit(EXIT_SUCCESS);
}
