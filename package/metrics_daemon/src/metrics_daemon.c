/*
 * Copyright (C) 2018 Swift Navigation Inc.
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
#include <string.h>
#include <unistd.h>
#include <ftw.h>

#include <json-c/json.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings.h>

#define PROGRAM_NAME "metrics_daemon"

#define METRICS_ROOT_DIRECTORY "/tmp/metrics/"
#define METRICS_OUTPUT_FILENAME "messaging_statistics-metrics.json"
#define METRICS_OUTPUT_FULLPATH  METRICS_ROOT_DIRECTORY METRICS_OUTPUT_FILENAME
#define METRICS_USAGE_UPDATE_INTERVAL (10000u)

const char *metrics_path = METRICS_ROOT_DIRECTORY;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMain options");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_METRICS_PATH = 1
  };

  const struct option long_opts[] = {
    {"metrics_path", required_argument, 0, OPT_ID_METRICS_PATH},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case OPT_ID_METRICS_PATH: {
        metrics_path = optarg;
      }
      break;

      default: {
        puts("Invalid option");
        return -1;
      }
      break;
    }
  }

  return 0;
}

static int handle_walk_path(const char *fpath, const struct stat *sb, int tflag)
{
  piksi_log(LOG_DEBUG, "%-3s %7jd   %-40s",
      (tflag == FTW_D) ?   "d"   : (tflag == FTW_DNR) ? "dnr" :
      (tflag == FTW_F) ?   "f" : (tflag == FTW_NS) ?  "ns"  : "???",
      (intmax_t) sb->st_size, fpath);
  // For each folder collect metrics from files
  // For each folder within the folder recurse

  return 0; // To tell ftw() to continue
}

/**
 * @brief write_messaging_metrics_to_file
 *
 * Write messaging metrics to file
 * @param metrics_dir: directory that holds the metrics directory tree structure
 */
static void write_messaging_metrics_to_file(const char *metrics_dir)
{
  (void)metrics_dir;
  /* Logic to write metrics info here */
  piksi_log(LOG_DEBUG, "Logging metrics to file: %s", METRICS_OUTPUT_FULLPATH);
  // Walk dir for metrics
  if (ftw(metrics_dir, handle_walk_path, 20) == -1) {
    perror("ftw");
    return;
  }
  // Write structure to file

  struct json_object *jobj = json_object_new_object();
  json_object_object_add(jobj, "question", json_object_new_string("A question"));
  json_object_object_add(jobj, "answer", json_object_new_string("An answer"));
  piksi_log(LOG_DEBUG, "%s\n---\n", json_object_to_json_string(jobj));
}

/**
 * @brief usage_timer_callback - used to trigger usage updates
 */
static void usage_timer_callback(pk_loop_t *loop, void *timer_handle, void *context)
{
  (void)loop;
  (void)timer_handle;
  (void)context;

  write_messaging_metrics_to_file(metrics_path);
}

static void signal_handler(pk_loop_t *pk_loop, void *handle, void *context)
{
  (void)context;
  int signal_value = pk_loop_get_signal_from_handle(handle);

  piksi_log(LOG_DEBUG, "Caught signal: %d", signal_value);

  pk_loop_stop(pk_loop);
}

static int cleanup(pk_loop_t **pk_loop_loc,
                   int status);

int main(int argc, char *argv[])
{
  pk_loop_t *loop = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    return cleanup(&loop, EXIT_FAILURE);
  }

  loop = pk_loop_create();
  if (loop == NULL) {
    return cleanup(&loop, EXIT_FAILURE);
  }

  if (pk_loop_signal_handler_add(loop, SIGINT, signal_handler, NULL) == NULL) {
    piksi_log(LOG_ERR, "Failed to add SIGINT handler to loop");
  }

  if (pk_loop_timer_add(loop, METRICS_USAGE_UPDATE_INTERVAL, usage_timer_callback, NULL) == NULL) {
    return cleanup(&loop, EXIT_FAILURE);
  }

  pk_loop_run_simple(loop);
  piksi_log(LOG_DEBUG, "Metrics Daemon: Normal Exit");

  return cleanup(&loop, EXIT_SUCCESS);
}

static int cleanup(pk_loop_t **pk_loop_loc,
                   int status) {
  pk_loop_destroy(pk_loop_loc);
  logging_deinit();

  return status;
}
