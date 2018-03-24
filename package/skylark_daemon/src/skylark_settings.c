/*
 * Copyright (C) 2017-2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/logging.h>

#include "skylark_settings.h"

#define UPLOAD_FIFO_FILE_PATH   "/var/run/skylark/upload"
#define DOWNLOAD_FIFO_FILE_PATH "/var/run/skylark/download"
#define SKYLARK_URL             "https://broker.skylark2.swiftnav.com"

static bool skylark_enabled;
static char skylark_url[256];

typedef struct {
  int (*execfn)(void);
  int pid;
} skylark_process_t;

static char *get_skylark_url(void) {
  return strcmp(skylark_url, "") == 0 ? SKYLARK_URL : skylark_url;
}

static int skylark_upload_daemon_execfn(void) {
  char *url = get_skylark_url();
  char *argv[] = {
    "skylark_daemon",
    "--upload",
    "--file", UPLOAD_FIFO_FILE_PATH,
    "--url", url,
    NULL,
  };

  return execvp(argv[0], argv);
}

static int skylark_upload_adapter_execfn(void) {
  char *argv[] = {
    "zmq_adapter",
    "--file", UPLOAD_FIFO_FILE_PATH,
    "-s", ">tcp://127.0.0.1:43080",
    "--filter-out", "sbp",
    "--filter-out-config", "/etc/skylark_upload_filter_out_config",
    NULL,
  };

  return execvp(argv[0], argv);
}

static int skylark_download_daemon_execfn(void) {
  char *url = get_skylark_url();
  char *argv[] = {
    "skylark_daemon",
    "--download",
    "--file", DOWNLOAD_FIFO_FILE_PATH,
    "--url", url,
    NULL,
  };

  return execvp(argv[0], argv);
}

static int skylark_download_adapter_execfn(void) {
  char *argv[] = {
    "zmq_adapter",
    "-f", "sbp",
    "--file", DOWNLOAD_FIFO_FILE_PATH,
    "-p", ">tcp://127.0.0.1:43081",
    NULL,
  };

  return execvp(argv[0], argv);
}

static skylark_process_t skylark_processes[] = {
  { .execfn = skylark_upload_daemon_execfn },
  { .execfn = skylark_upload_adapter_execfn },
  { .execfn = skylark_download_adapter_execfn },
  { .execfn = skylark_download_daemon_execfn },
};

static const int skylark_processes_count =
  sizeof(skylark_processes)/sizeof(skylark_processes[0]);

static int skylark_notify(void *context)
{
  (void)context;

  for (int i=0; i<skylark_processes_count; i++) {
    skylark_process_t *process = &skylark_processes[i];

    if (process->pid != 0) {
      int ret = kill(process->pid, SIGTERM);
      if (ret != 0) {
        piksi_log(LOG_ERR, "kill pid %d error (%d) \"%s\"",
                  process->pid, errno, strerror(errno));
      }
      process->pid = 0;
    }

    if (!skylark_enabled) {
      system("echo 0 >/var/run/skylark/enabled");
      continue;
    }

    system("echo 1 >/var/run/skylark/enabled");

    process->pid = fork();
    if (process->pid == 0) {
      process->execfn();
      piksi_log(LOG_ERR, "exec error (%d) \"%s\"", errno, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}

bool skylark_reconnect_dl(void)
{
  for (int i=0; i<skylark_processes_count; i++) {

    skylark_process_t *process = &skylark_processes[i];

    if (process->execfn == skylark_download_daemon_execfn) {

      if (process->pid == 0) {
        piksi_log(LOG_ERR, "Asked to tell skylark_download_daemon to reconnect, but it isn't running");
        return false;
      }

      int ret = kill(process->pid, SIGUSR1);

      if (ret != 0) {
        piksi_log(LOG_ERR, "skylark_reconnect_dl: kill (SIGUSR1) pid %d error (%d) \"%s\"",
                  process->pid, errno, strerror(errno));

        return false;
      }
    }
  }

  return true;
}

void skylark_init(settings_ctx_t *settings_ctx)
{
  mkfifo(UPLOAD_FIFO_FILE_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  mkfifo(DOWNLOAD_FIFO_FILE_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  settings_register(settings_ctx, "skylark", "enable",
                    &skylark_enabled, sizeof(skylark_enabled),
                    SETTINGS_TYPE_BOOL,
                    skylark_notify, NULL);

  settings_register(settings_ctx, "skylark", "url",
                    &skylark_url, sizeof(skylark_url),
                    SETTINGS_TYPE_STRING,
                    skylark_notify, NULL);
}
