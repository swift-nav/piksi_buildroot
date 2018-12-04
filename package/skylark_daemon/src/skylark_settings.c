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

#include <unistd.h>
#include <errno.h>

#include <sys/stat.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings_client.h>
#include <libpiksi/util.h>

#include "skylark_settings.h"

// clang-format off
#define UPLOAD_FIFO_FILE_PATH   "/var/run/skylark/upload"
#define DOWNLOAD_FIFO_FILE_PATH "/var/run/skylark/download"
#define SKYLARK_URL             "https://broker.skylark.swiftnav.com"
// clang-format on

static bool skylark_enabled;
static char skylark_url[256];

typedef struct {
  int (*execfn)(void);
  int pid;
} skylark_process_t;

static char *get_skylark_url(void)
{
  return strcmp(skylark_url, "") == 0 ? SKYLARK_URL : skylark_url;
}

static int skylark_upload_daemon_execfn(void)
{
  char *url = get_skylark_url();
  char *argv[] = {
    "skylark_daemon",
    "--upload",
    "--no-error-reporting",
    "--file",
    UPLOAD_FIFO_FILE_PATH,
    "--url",
    url,
    NULL,
  };

  return execvp(argv[0], argv);
}

static int skylark_upload_adapter_execfn(void)
{
  char *argv[] = {
    "endpoint_adapter",
    "--name",
    "skylark_upload",
    "--file",
    UPLOAD_FIFO_FILE_PATH,
    "-s",
    "ipc:///var/run/sockets/skylark.pub",
    "--filter-out",
    "sbp",
    "--filter-out-config",
    "/etc/skylark_upload_filter_out_config",
    NULL,
  };

  return execvp(argv[0], argv);
}

static int skylark_download_daemon_execfn(void)
{
  char *url = get_skylark_url();
  char *argv[] = {
    "skylark_daemon",
    "--download",
    "--file",
    DOWNLOAD_FIFO_FILE_PATH,
    "--url",
    url,
    NULL,
  };

  return execvp(argv[0], argv);
}

static int skylark_download_adapter_execfn(void)
{
  char *argv[] = {
    "endpoint_adapter",
    "--name",
    "skylark_download",
    "-f",
    "sbp",
    "--file",
    DOWNLOAD_FIFO_FILE_PATH,
    "-p",
    "ipc:///var/run/sockets/skylark.sub",
    NULL,
  };

  return execvp(argv[0], argv);
}

static skylark_process_t skylark_processes[] = {
  {.pid = 0, .execfn = skylark_upload_daemon_execfn},
  {.pid = 0, .execfn = skylark_upload_adapter_execfn},
  {.pid = 0, .execfn = skylark_download_adapter_execfn},
  {.pid = 0, .execfn = skylark_download_daemon_execfn},
};

static const size_t skylark_processes_count = COUNT_OF(skylark_processes);

static void skylark_stop_process(size_t i)
{
  skylark_process_t *process = &skylark_processes[i];
  if (process->pid != 0) {
    int ret = kill(process->pid, SIGTERM);
    if (ret != 0) {
      piksi_log(LOG_ERR, "kill pid %d error (%d) \"%s\"", process->pid, errno, strerror(errno));
    }
    sleep(0.1); // allow us to receive sigchild
    process->pid = 0;
  }
}

void skylark_stop_processes()
{
  for (size_t i = 0; i < skylark_processes_count; i++) {
    skylark_stop_process(i);
  }
}

void skylark_record_exit(pid_t pid)
{
  for (size_t i = 0; i < skylark_processes_count; i++) {
    skylark_process_t *process = &skylark_processes[i];
    if (process->pid != 0 && process->pid == pid) {
      piksi_log(LOG_DEBUG, "known child process pid %d exited", process->pid);
      process->pid = 0;
      return;
    }
  }
}

static int skylark_notify(void *context)
{
  (void)context;

  for (size_t i = 0; i < skylark_processes_count; i++) {

    skylark_stop_process(i);

    if (!skylark_enabled) {
      system("echo 0 >/var/run/skylark/enabled");
      continue;
    }

    system("echo 1 >/var/run/skylark/enabled");

    skylark_process_t *process = &skylark_processes[i];
    process->pid = fork();

    if (process->pid == 0) {
      process->execfn();
      piksi_log(LOG_ERR, "exec error (%d) \"%s\"", errno, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  return SETTINGS_WR_OK;
}

bool skylark_reconnect_dl(void)
{
  for (size_t i = 0; i < skylark_processes_count; i++) {

    skylark_process_t *process = &skylark_processes[i];

    if (process->execfn == skylark_download_daemon_execfn) {

      if (process->pid == 0) {
        piksi_log(
          LOG_ERR,
          "Asked to tell skylark_daemon to reconnect (in download mode), but it isn't running");
        return false;
      }

      int ret = kill(process->pid, SIGUSR1);

      if (ret != 0) {
        piksi_log(LOG_ERR,
                  "skylark_reconnect_dl: kill (SIGUSR1) pid %d error (%d) \"%s\"",
                  process->pid,
                  errno,
                  strerror(errno));

        return false;
      }
    }
  }

  return true;
}

void skylark_init(pk_settings_ctx_t *settings_ctx)
{
  mkfifo(UPLOAD_FIFO_FILE_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  mkfifo(DOWNLOAD_FIFO_FILE_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  pk_settings_register(settings_ctx,
                       "skylark",
                       "enable",
                       &skylark_enabled,
                       sizeof(skylark_enabled),
                       SETTINGS_TYPE_BOOL,
                       skylark_notify,
                       NULL);

  pk_settings_register(settings_ctx,
                       "skylark",
                       "url",
                       &skylark_url,
                       sizeof(skylark_url),
                       SETTINGS_TYPE_STRING,
                       skylark_notify,
                       NULL);
}
