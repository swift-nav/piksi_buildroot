/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <unistd.h>
#include <libpiksi/logging.h>

#include "ntrip.h"

#define FIFO_FILE_PATH "/var/run/ntrip"

static bool ntrip_enabled;
static bool ntrip_debug;
static char ntrip_url[256];
static char ntrip_internval_s[16];

static int ntrip_internval = 0;

static char *ntrip_argv_normal[] = {
  "ntrip_daemon",
  "--file", FIFO_FILE_PATH,
  "--url", ntrip_url,
  "--interval", ntrip_internval_s,
  NULL,
};

static char *ntrip_argv_debug[] = {
  "ntrip_daemon",
  "--debug",
  "--file", FIFO_FILE_PATH,
  "--url", ntrip_url,
  "--interval", ntrip_internval_s,
  NULL,
};

static char** ntrip_argv = ntrip_argv_normal;

typedef struct {
  int (*execfn)(void);
  int pid;
} ntrip_process_t;

static int ntrip_daemon_execfn(void) {
  return execvp(ntrip_argv[0], ntrip_argv);
}

static int ntrip_adapter_execfn(void) {
  char *argv[] = {
    "zmq_adapter",
    "-f", "rtcm3",
    "--file", FIFO_FILE_PATH,
    "-p", ">tcp://127.0.0.1:45031",
    NULL,
  };

  return execvp(argv[0], argv);
}

static ntrip_process_t ntrip_processes[] = {
  { .execfn = ntrip_adapter_execfn },
  { .execfn = ntrip_daemon_execfn },
};

static const int ntrip_processes_count =
  sizeof(ntrip_processes)/sizeof(ntrip_processes[0]);

static int ntrip_notify(void *context)
{
  (void)context;

  snprintf(ntrip_internval_s, sizeof(ntrip_internval_s), "%d", ntrip_internval);

  for (int i=0; i<ntrip_processes_count; i++) {
    ntrip_process_t *process = &ntrip_processes[i];

    if (process->pid != 0) {
      int ret = kill(process->pid, SIGTERM);
      if (ret != 0) {
        piksi_log(LOG_ERR, "kill pid %d error (%d) \"%s\"",
                  process->pid, errno, strerror(errno));
      }
      sleep(1.0);
      ret = kill(process->pid, SIGKILL);
      if (ret != 0 && errno != ESRCH) {
        piksi_log(LOG_ERR, "force kill pid %d error (%d) \"%s\"",
                  process->pid, errno, strerror(errno));
      }
      process->pid = 0;
    }

    if (!ntrip_enabled || strcmp(ntrip_url, "") == 0) {
      system("echo 0 >/var/run/ntrip_enabled");
      continue;
    }

    system("echo 1 >/var/run/ntrip_enabled");

    ntrip_argv = ntrip_debug ? ntrip_argv_debug : ntrip_argv_normal;

    process->pid = fork();
    if (process->pid == 0) {
      process->execfn();
      piksi_log(LOG_ERR, "exec error (%d) \"%s\"", errno, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}

void ntrip_init(settings_ctx_t *settings_ctx)
{
  mkfifo(FIFO_FILE_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  settings_register(settings_ctx, "ntrip", "enable",
                    &ntrip_enabled, sizeof(ntrip_enabled),
                    SETTINGS_TYPE_BOOL,
                    ntrip_notify, NULL);

  settings_register(settings_ctx, "ntrip", "debug",
                    &ntrip_debug, sizeof(ntrip_debug),
                    SETTINGS_TYPE_BOOL,
                    ntrip_notify, NULL);

  settings_register(settings_ctx, "ntrip", "url",
                    &ntrip_url, sizeof(ntrip_url),
                    SETTINGS_TYPE_STRING,
                    ntrip_notify, NULL);

  settings_register(settings_ctx, "ntrip", "interval",
                    &ntrip_internval, sizeof(ntrip_internval),
                    SETTINGS_TYPE_INT,
                    ntrip_notify, NULL);
}

void ntrip_reconnect() {

  for (int i=0; i<ntrip_processes_count; i++) {

    ntrip_process_t *process = &ntrip_processes[i];

    if (process->execfn == ntrip_daemon_execfn) {

      if (process->pid == 0) {
        piksi_log(LOG_ERR, "Asked to tell ntrip_daemon to reconnect, but it isn't running");
        return;
      }

      int ret = kill(process->pid, SIGUSR1);

      if (ret != 0) {
        piksi_log(LOG_ERR, "ntrip_reconnect: kill pid %d error (%d) \"%s\"",
                  process->pid, errno, strerror(errno));
      }
    }
  }
}
