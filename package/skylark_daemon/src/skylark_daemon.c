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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/wait.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include <libnetwork.h>

#include "skylark_settings.h"

#define PROGRAM_NAME "skylark_daemon"

#define SKYLARK_CONTROL_FILE "/var/run/skylark/control/socket"
#define SKYLARK_CONTROL_SOCK "ipc://" SKYLARK_CONTROL_FILE

#define SKYLARK_CONTROL_COMMAND_RECONNECT "r"

#define MSG_GET_HEALTH_ERROR "Error requesting skylark connection HTTP response code: %d"
#define MSG_GET_HEALTH_ERROR_LF (MSG_GET_HEALTH_ERROR "\n")

static bool debug = false;
static const char *fifo_file_path = NULL;
static const char *url = NULL;
static bool no_error_reporting = false;

typedef enum {
  OP_MODE_NONE,
  OP_MODE_DOWNLOAD,
  OP_MODE_UPLOAD,
  OP_MODE_SETTINGS,
  OP_MODE_RECONNECT_DL,
  OP_MODE_GET_HEALTH,
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
  puts("\t--get-health     Ask the download daemon for it's last HTTP response code");

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
    OPT_ID_RECONNECT_DL,
    OPT_ID_NO_ERROR_REPORTING,
    OPT_ID_GET_HEALTH,
  };

  // clang-format off
  const struct option long_opts[] = {
    {"upload",             no_argument,       0, OPT_ID_UPLOAD},
    {"download",           no_argument,       0, OPT_ID_DOWNLOAD},
    {"settings",           no_argument,       0, OPT_ID_SETTINGS},
    {"reconnect-dl",       no_argument,       0, OPT_ID_RECONNECT_DL},
    {"no-error-reporting", no_argument,       0, OPT_ID_NO_ERROR_REPORTING},
    {"get-health",         no_argument,       0, OPT_ID_GET_HEALTH},
    {"file",               required_argument, 0, OPT_ID_FILE},
    {"url  ",              required_argument, 0, OPT_ID_URL},
    {"debug",              no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };
  // clang-format on

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {

      case OPT_ID_UPLOAD: {
        op_mode = OP_MODE_UPLOAD;
      }
      break;

      case OPT_ID_DOWNLOAD: {
        op_mode = OP_MODE_DOWNLOAD;
      }
      break;

      case OPT_ID_SETTINGS: {
        op_mode = OP_MODE_SETTINGS;
      }
      break;

      case OPT_ID_RECONNECT_DL: {
        op_mode = OP_MODE_RECONNECT_DL;
      }
      break;

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

      case OPT_ID_NO_ERROR_REPORTING: {
        no_error_reporting = true;
      }
      break;

      case OPT_ID_GET_HEALTH: {
        op_mode = OP_MODE_GET_HEALTH;
      }
      break;

      default: {
        puts("Invalid option");
        return -1;
      }
      break;
    }
  }

  if (op_mode == OP_MODE_DOWNLOAD || op_mode == OP_MODE_UPLOAD) {
    if (fifo_file_path == NULL) {
      puts("Missing file");
      return -1;
    }
    if (url == NULL) {
      puts("Missing url");
      return -1;
    }
  }

  return 0;
}

static void network_terminate_handler(int signum, siginfo_t *info, void *ucontext)
{
  (void) ucontext;

  piksi_log(LOG_DEBUG, "%s: received signal: %d, sender: %d",
            __FUNCTION__, signum, info->si_pid);

  libnetwork_shutdown();
}

static void settings_loop_sigchild()
{
  reap_children(debug, skylark_record_exit);
}

static void cycle_connection(int signum)
{
  piksi_log(LOG_DEBUG, "%s: received signal: %d", __FUNCTION__, signum);
  libnetwork_cycle_connection();
}

static bool configure_libnetwork(network_context_t* ctx, int fd, operating_mode mode)
{
  network_status_t status = NETWORK_STATUS_SUCCESS;

  if ((status = libnetwork_set_url(ctx, url)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if ((status = libnetwork_set_fd(ctx, fd)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if ((status = libnetwork_set_debug(ctx, debug)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;
  if (mode == OP_MODE_DOWNLOAD && (status = libnetwork_configure_control(ctx, SKYLARK_CONTROL_PAIR)) != NETWORK_STATUS_SUCCESS)
    goto exit_error;

  if (no_error_reporting) {
    libnetwork_report_errors(ctx, false);
  }

  return true;

exit_error:
  piksi_log(LOG_ERR, "error configuring the libnetwork context: %d", status);
  return false;
}

static void skylark_upload_mode()
{
  int fd = open(fifo_file_path, O_RDONLY);
  if (fd < 0) {
    piksi_log(LOG_ERR, "fifo error (%d) \"%s\"", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  network_context_t* network_context = libnetwork_create(NETWORK_TYPE_SKYLARK_UPLOAD);
  if(!configure_libnetwork(network_context, fd, OP_MODE_UPLOAD)) {
    exit(EXIT_FAILURE);
  }

  setup_sigint_handler(network_terminate_handler);
  setup_sigterm_handler(network_terminate_handler);

  skylark_upload(network_context);

  close(fd);
  libnetwork_destroy(&network_context);
}

static int skylark_request_health()
{
  int response_code = -1;
  network_status_t status = libnetwork_request_health(SKYLARK_CONTROL_PAIR, &response_code);

  if (status != NETWORK_STATUS_SUCCESS) {
    piksi_log(LOG_ERR, MSG_GET_HEALTH_ERROR, response_code);
    fprintf(stderr, MSG_GET_HEALTH_ERROR_LF, response_code);
    return EXIT_FAILURE;
  }

  fprintf(stdout, "%03d\n", response_code);
  return EXIT_SUCCESS;
}

static void skylark_download_mode()
{
  int fd = open(fifo_file_path, O_WRONLY);
  if (fd < 0) {
    piksi_log(LOG_ERR, "fifo error (%d) \"%s\"", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  network_context_t* network_context = libnetwork_create(NETWORK_TYPE_SKYLARK_DOWNLOAD);

  if(!configure_libnetwork(network_context, fd, OP_MODE_DOWNLOAD)) {
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

  setup_sigint_handler(network_terminate_handler);
  setup_sigterm_handler(network_terminate_handler);

  skylark_download(network_context);

  close(fd);
  libnetwork_destroy(&network_context);
}

static void settings_loop_terminate()
{
  skylark_stop_processes();
  reap_children(debug, NULL);
}

static void skylark_settings_loop(void)
{
  settings_loop(SKYLARK_CONTROL_SOCK,
                SKYLARK_CONTROL_FILE,
                SKYLARK_CONTROL_COMMAND_RECONNECT,
                skylark_init,
                skylark_reconnect_dl,
                settings_loop_terminate,
                settings_loop_sigchild);
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  int exit_status = EXIT_SUCCESS;

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
  case OP_MODE_GET_HEALTH:
    exit_status = skylark_request_health();
    break;
  case OP_MODE_RECONNECT_DL:
    settings_loop_send_command("Skylark upload client",
                               SKYLARK_CONTROL_COMMAND_RECONNECT,
                               "reconnect",
                               SKYLARK_CONTROL_SOCK);
    break;
  case OP_MODE_NONE:
  default:
    {
      const char* error_msg = "No operating mode selected";
      piksi_log(LOG_ERR, error_msg);
      fprintf(stderr, "%s\n", error_msg);
      usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  logging_deinit();
  exit(exit_status);
}
