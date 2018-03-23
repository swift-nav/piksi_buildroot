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
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>
#include <libnetwork.h>

#include "ntrip_settings.h"

#define PUB_ENDPOINT ">tcp://127.0.0.1:43011"
#define SUB_ENDPOINT ">tcp://127.0.0.1:43010"

#define NTRIP_CONTROL_FILE "/var/run/ntrip/control/socket"
#define NTRIP_CONTROL_SOCK "ipc://" NTRIP_CONTROL_FILE

#define NTRIP_CONTROL_COMMAND_RECONNECT "r"

#define PROGRAM_NAME "ntrip_daemon"

static bool debug = false;
static const char *fifo_file_path = NULL;
static const char *username = NULL;
static const char *password = NULL;
static const char *url = NULL;

static double gga_xfer_secs = 0.0;

typedef enum {
  OP_MODE_NTRIP_CLIENT,
  OP_MODE_SETTINGS_DAEMON,
  OP_MODE_REQ_RECONNECT,
} operating_mode;

static operating_mode op_mode = OP_MODE_NTRIP_CLIENT;

// In processes Socket used to quit the settings daemon if a SIGTERM/SIGINT is
//   received so we can cleanly remove our control socket.
static zsock_t* loop_quit = NULL;

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
    return -1;
  }

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

static int control_handler_cleanup(char* data, int rc)
{
  free(data);
  return rc;
}

static int control_handler(zloop_t *zloop, zsock_t *zsock, void *arg)
{
  (void)zloop;
  (void)arg;

  char *data;

  piksi_log(LOG_DEBUG, "Waiting to receive data...");

  int rc = zsock_recv(zsock, "s", &data);
  if ( rc != 0 ) {
    piksi_log(LOG_WARNING, "zsock_recv error: %s", zmq_strerror(zmq_errno()));
    return 0;
  }

  u32 length = strlen(data);

  if (length == 0 || length > 1) {
    piksi_log(LOG_WARNING, "command had invalid length: %u", length);
    return control_handler_cleanup(data, 0);
  }

  if (data == NULL) {
    piksi_log(LOG_WARNING, "received data was null");
    return control_handler_cleanup(data, 0);
  }

  if (*data != NTRIP_CONTROL_COMMAND_RECONNECT[0]) {
    piksi_log(LOG_WARNING, "command had invalid value: %c", *data);
    return control_handler_cleanup(data, 0);
  }

  piksi_log(LOG_INFO, "Read request to refresh ntrip connection...");
  u8 result = ntrip_reconnect() ? 1 : 0;

  zsock_send(zsock, "1", result);

  return control_handler_cleanup(data, 0);
}

static void signal_handler (int signal_value)
{
  piksi_log(LOG_DEBUG, "Caught signal: %d", signal_value);

  if (loop_quit != NULL)
    zsock_send(loop_quit, "1", 1);
  else
    piksi_log(LOG_DEBUG, "Loop quit socket was null...");
}

static void cleanup_signal_handlers (void)
{
  zsock_destroy(&loop_quit);
}

static void setup_signal_handlers ()
{
  loop_quit = zsock_new_pair("@inproc://loop_quit");

  if (loop_quit == NULL) {
    piksi_log(LOG_ERR, "Error creating loop quit socket: %s",
              zmq_strerror(zmq_errno()));
    return;
  }

  struct sigaction action;

  action.sa_handler = signal_handler;
  action.sa_flags = 0;

  sigemptyset (&action.sa_mask);

  sigaction (SIGINT, &action, NULL);
  sigaction (SIGTERM, &action, NULL);
}

static int loop_quit_handler(zloop_t *zloop, zsock_t *zsock, void *arg)
{
  (void) zloop;
  (void) arg;

  u8 request = 0;
  zsock_recv(zsock, "1", &request);

  assert( request == 1 );

  piksi_log(LOG_INFO, "Shutting down...");
  return -1;
}

static int settings_loop(void)
{
  piksi_log(LOG_INFO, "Starting daemon mode for NTRIP settings...");

  zsys_handler_set(NULL);
  setup_signal_handlers();

  /* Set up SBP ZMQ */
  sbp_zmq_pubsub_ctx_t *pubsub_ctx = sbp_zmq_pubsub_create(PUB_ENDPOINT,
                                                           SUB_ENDPOINT);
  if (pubsub_ctx == NULL) {
    return -1;
  }

  /* Set up settings */
  settings_ctx_t *settings_ctx = settings_create();
  if (settings_ctx == NULL) {
    return -1;
  }

  if (settings_reader_add(settings_ctx,
                          sbp_zmq_pubsub_zloop_get(pubsub_ctx)) != 0) {
    return -1;
  }

  ntrip_init(settings_ctx);

  zloop_t* loop = sbp_zmq_pubsub_zloop_get(pubsub_ctx);
  zsock_t* rep_socket = zsock_new_rep(NTRIP_CONTROL_SOCK);

  int rc = chmod(NTRIP_CONTROL_FILE, 0777);
  if (rc != 0) {
    piksi_log(LOG_ERR, "Error configuring IPC pipe permissions: %s",
              strerror(errno));
    return -1;
  }

  if (rep_socket == NULL) {
    const char* err_msg = zmq_strerror(zmq_errno());
    piksi_log(LOG_ERR, "Error creating IPC control path: %s, error: %s",
              NTRIP_CONTROL_SOCK, err_msg);
    return -1;
  }

  if (zloop_reader(loop, rep_socket, control_handler, NULL) < 0) {
    const char* err_msg = zmq_strerror(zmq_errno());
    piksi_log(LOG_ERR, "Error registering reader: %s", err_msg);
    return -1;
  }

  zsock_t* loop_quit_reader = zsock_new_pair(">inproc://loop_quit");

  if (loop_quit == NULL) {
    piksi_log(LOG_ERR, "Error creating loop quit socket: %s",
              zmq_strerror(zmq_errno()));
    return -1;
  }

  if (zloop_reader(loop, loop_quit_reader, loop_quit_handler, NULL) < 0) {
    const char* err_msg = zmq_strerror(zmq_errno());
    piksi_log(LOG_ERR, "Error registering reader: %s", err_msg);
    return -1;
  }

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(pubsub_ctx));

  zsock_destroy(&rep_socket);

  zsock_destroy(&loop_quit_reader);
  cleanup_signal_handlers();

  sbp_zmq_pubsub_destroy(&pubsub_ctx);
  settings_destroy(&settings_ctx);

  return 0;
}

static int request_reconnect(void)
{
  const char* msg = "Requesting refresh of NTRIP client connection...";

  piksi_log(LOG_INFO, msg);
  printf("%s\n", msg);

  zsock_t* req_socket = zsock_new_req(NTRIP_CONTROL_SOCK);
  if (req_socket == NULL) {
    piksi_log(LOG_ERR, "request_reconnect: error in zsock_new_pub");
    return -1;
  }

  int ret = zsock_send(req_socket, "s", NTRIP_CONTROL_COMMAND_RECONNECT);

  if (ret != 0) {
    piksi_log(LOG_WARNING, "request_reconnect: error sending message");
    return ret;
  }

  u8 result = 0;
  zsock_recv(req_socket, "1", result);

# define NTRIP_REFRESH_RESULT "Result of NTRIP connection refresh: %hhu"

  piksi_log(LOG_INFO, NTRIP_REFRESH_RESULT, result);
  printf(NTRIP_REFRESH_RESULT "\n", result);

  zsock_destroy(&req_socket);

  return 0;
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
    ret = settings_loop();
    break;
  case OP_MODE_REQ_RECONNECT:
    ret = request_reconnect();
    break;
  default:
    assert(false);
  }

  logging_deinit();

  if (ret != 0)
    exit(EXIT_FAILURE);

  exit(EXIT_SUCCESS);
}
