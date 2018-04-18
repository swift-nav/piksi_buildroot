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

#include <libpiksi/settings.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "rotating_logger.h"

#define PROGRAM_NAME "standalone_file_logger"

#define SLICE_DURATION_DEFAULT_m 10
#define POLL_PERIOD_DEFAULT_s 30
#define FILL_THRESHOLD_DEFAULT_p 95

static const char* const logging_filesystem_names[] = {
  "FAT", "F2FS", NULL
};

typedef enum {
  LOGGING_FILESYSTEM_FAT,
  LOGGING_FILESYSTEM_F2FS,
} logging_fs_t;

static struct { bool is_set; logging_fs_t value; } logging_fs_type_prev = { false, LOGGING_FILESYSTEM_FAT };
static logging_fs_t logging_fs_type = LOGGING_FILESYSTEM_FAT;

static bool copy_system_logs_enable = false;

static int poll_period_s = POLL_PERIOD_DEFAULT_s;

static const char *zmq_sub_endpoint = nullptr;

static RotatingLogger* logger = nullptr;

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

static bool setting_usb_logging_enable = false;
static const int MAX_PATH_LEN = 1024;
static char setting_usb_logging_dir[MAX_PATH_LEN] = {0};
static int setting_usb_logging_max_fill = FILL_THRESHOLD_DEFAULT_p;
static int setting_usb_logging_slice_duration = SLICE_DURATION_DEFAULT_m;

static int parse_options(int argc, char *argv[]) {
  enum { OPT_ID_DURATION = 1, OPT_ID_PERIOD, OPT_ID_THRESHOLD, OPT_ID_FLUSH };

  const struct option long_opts[] = {
      {"sub", required_argument, nullptr, 's'},
      {"dir", required_argument, nullptr, 'd'},
      {"slice-duration", required_argument, nullptr, OPT_ID_DURATION},
      {"poll-period", required_argument, nullptr, OPT_ID_PERIOD},
      {"full-threshold", required_argument, nullptr, OPT_ID_THRESHOLD},
      {nullptr, 0, nullptr, 0}};

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "s:d:", long_opts, &opt_index)) != -1) {
    switch (c) {
      case 's': {
        zmq_sub_endpoint = optarg;
      } break;

      case 'd': {
        if (strlen(optarg) >= MAX_PATH_LEN) {
          puts("Log output path too long");
          return -1;
        }
        strcpy(setting_usb_logging_dir, optarg);
      } break;

      case OPT_ID_DURATION: {
        setting_usb_logging_slice_duration = strtol(optarg, nullptr, 10);
      } break;

      case OPT_ID_PERIOD: {
        poll_period_s = strtol(optarg, nullptr, 10);
      } break;

      case OPT_ID_THRESHOLD: {
        setting_usb_logging_max_fill = strtol(optarg, nullptr, 10);
      } break;

      default: {
        piksi_log(LOG_ERR, "invalid option");
        return -1;
      } break;
    }
  }

  if (zmq_sub_endpoint == nullptr) {
    piksi_log(LOG_ERR, "Must specify source");
    return -1;
  }

  if (strlen(setting_usb_logging_dir) == 0) {
    piksi_log(LOG_ERR, "Must specify sink directory");
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

static void process_log_callback(int priority, const char *msg_text)
{
  piksi_log(priority|LOG_SBP, msg_text);
}

static void stop_logging()
{
  if (logger != nullptr) {
    process_log_callback(LOG_INFO, "Logging stopped");
    delete logger;
    logger = nullptr;
  }
}

static void save_prev_logging_fs_type_value()
{
  logging_fs_type_prev.value = logging_fs_type;
  logging_fs_type_prev.is_set = true;
}

static int logging_filesystem_notify(void* context)
{
  (void) context;

  piksi_log(LOG_DEBUG, "%s: curr=%d, prev=%d (set=%hhu)\n", __FUNCTION__, logging_fs_type, logging_fs_type_prev.value, logging_fs_type_prev.is_set);

  if (logging_fs_type != LOGGING_FILESYSTEM_F2FS) {
    save_prev_logging_fs_type_value();
    return 0;
  }

  if (!logging_fs_type_prev.is_set || logging_fs_type_prev.value != LOGGING_FILESYSTEM_FAT) {
    save_prev_logging_fs_type_value();
    return 0;
  }

  save_prev_logging_fs_type_value();

  const int str_count = 6;
  const int str_max = 128;

  const char warning_strs[str_count][str_max] = {
    "Logging file-system: ************************************************************",
    "Logging file-system: Detected that the logging file-system was changed to F2FS...",
    "Logging file-system: ... this will ERASE any removable media attached to system!",
    "Logging file-system: The file-system will be reformatted on the next reboot...",
    "Logging file-system: ...settings must be persisted for this to take effect.",
    "Logging file-system: ************************************************************"
  };

  for (size_t x = str_count; x > 0; --x)
    sbp_log(LOG_WARNING, warning_strs[x-1]);

  for (size_t x = 0; x < str_count; ++x)
    piksi_log(LOG_WARNING, warning_strs[x]);

  return 0;
}

static int copy_system_logs_notify(void* context)
{
  if (copy_system_logs_enable) {
    system("COPY_SYS_LOGS=y /etc/init.d/S98copy_sys_logs start");
  } else {
    system("COPY_SYS_LOGS= /etc/init.d/S98copy_sys_logs stop");
  }

  return 0;
}

static int setting_usb_logging_notify(void *context)
{
  (void *)context;

  if (setting_usb_logging_enable) {
    if (logger == nullptr) {
      process_log_callback(LOG_INFO, "Logging started");
      logger = new RotatingLogger(setting_usb_logging_dir, setting_usb_logging_slice_duration,
                                  poll_period_s, setting_usb_logging_max_fill,
                                  &process_log_callback);
    } else {
      logger->update_dir(setting_usb_logging_dir);
      logger->update_fill_threshold(setting_usb_logging_max_fill);
      logger->update_slice_duration(setting_usb_logging_slice_duration);
    }
  } else {
    stop_logging();
  }

  return 0;
}

static void terminate_handler(int signum) {

  stop_logging();

  /* Send this signal to the entire process group */
  killpg(0, signum);

  /* Exit */
  _exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  setpgid(0, 0); /* Set PGID = PID */

  logging_init(PROGRAM_NAME);

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
    piksi_log(LOG_ERR, "error setting up sigchld handler");
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
    piksi_log(LOG_ERR, "error setting up terminate handler");
    exit(EXIT_FAILURE);
  }

  zmq_pollitem_t items[2];

  zsock_t * zmq_sub = zsock_new_sub(zmq_sub_endpoint, "");
  if (zmq_sub == nullptr) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(EXIT_FAILURE);
  }
  items[0] = (zmq_pollitem_t) {
    .socket = zsock_resolve(zmq_sub),
    .fd = 0,
    .events = ZMQ_POLLIN,
    .revents = 0
  };

  /* Set up settings */
  settings_ctx_t *settings_ctx = settings_create();
  if (settings_ctx == nullptr) {
    exit(EXIT_FAILURE);
  }
  /* Register settings */
  settings_register(settings_ctx, "standalone_logging", "enable", &setting_usb_logging_enable,
                    sizeof(setting_usb_logging_enable), SETTINGS_TYPE_BOOL,
                    &setting_usb_logging_notify, nullptr);
  settings_register(settings_ctx, "standalone_logging", "output_directory", &setting_usb_logging_dir,
                    sizeof(setting_usb_logging_dir), SETTINGS_TYPE_STRING,
                    &setting_usb_logging_notify, nullptr);
  settings_register(settings_ctx, "standalone_logging", "max_fill", &setting_usb_logging_max_fill,
                    sizeof(setting_usb_logging_max_fill), SETTINGS_TYPE_INT,
                    &setting_usb_logging_notify, nullptr);
  settings_register(settings_ctx, "standalone_logging", "file_duration", &setting_usb_logging_slice_duration,
                    sizeof(setting_usb_logging_slice_duration), SETTINGS_TYPE_INT,
                    &setting_usb_logging_notify, nullptr);
  settings_pollitem_init(settings_ctx, &items[1]);

  settings_type_t settings_type_logging_filesystem;
  settings_type_register_enum(settings_ctx,
                              logging_filesystem_names,
                              &settings_type_logging_filesystem);

  settings_register(settings_ctx, "standalone_logging", "logging_file_system",
                    &logging_fs_type, sizeof(logging_fs_type), settings_type_logging_filesystem,
                    logging_filesystem_notify, nullptr);
  settings_register(settings_ctx, "standalone_logging", "copy_system_logs", &copy_system_logs_enable,
                    sizeof(copy_system_logs_enable), SETTINGS_TYPE_BOOL,
                    &copy_system_logs_notify, nullptr);

  process_log_callback(LOG_INFO, "Starting");

  while (true) {
    int ret = zmq_poll(items, 2, -1);

    if (ret == -1) {
      piksi_log(LOG_ERR, "poll failed");
      exit(EXIT_FAILURE);
    }

    if (items[0].revents & ZMQ_POLLIN) {
      zmsg_t *msg = zmsg_recv(zmq_sub);
      if (msg == nullptr) {
        continue;
      }
      if (logger != nullptr) {
        zframe_t *frame;
        for (frame = zmsg_first(msg); frame != nullptr; frame = zmsg_next(msg)) {
          logger->frame_handler(zframe_data(frame), zframe_size(frame));
        }
      }
      zmsg_destroy(&msg);
    }

    settings_pollitem_check(settings_ctx, &items[1]);
  }

  zsock_destroy(&zmq_sub);
  settings_destroy(&settings_ctx);

  exit(EXIT_SUCCESS);
}
