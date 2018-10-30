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

#include <errno.h>
#include <unistd.h>

#include <sys/stat.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>

#include "ntrip_settings.h"

#define FIFO_FILE_PATH "/var/run/ntrip/fifo"
#define NTRIP_ENABLED_FILE_PATH "/var/run/ntrip/enabled"

static bool ntrip_enabled;
static bool ntrip_debug;
static char ntrip_username[256];
static char ntrip_password[256];
static char ntrip_url[256];
static int ntrip_interval = 0;
static char ntrip_interval_s[16];
static bool ntrip_rev1gga = false;
static char ntrip_rev1gga_s[] = "n";

static bool ntrip_settings_initialized = false;

// clang-format off
static char *ntrip_argv_normal[] = {
  "ntrip_daemon",
  "--file", FIFO_FILE_PATH,
  "--url", ntrip_url,
  "--interval", ntrip_interval_s,
  "--rev1gga", ntrip_rev1gga_s,
  NULL,
};

static char *ntrip_argv_debug[] = {
  "ntrip_daemon",
  "--debug",
  "--file", FIFO_FILE_PATH,
  "--url", ntrip_url,
  "--interval", ntrip_interval_s,
  "--rev1gga", ntrip_rev1gga_s,
  NULL,
};

static char *ntrip_argv_username[] = {
  "ntrip_daemon",
  "--file", FIFO_FILE_PATH,
  "--username", ntrip_username,
  "--password", ntrip_password,
  "--url", ntrip_url,
  "--interval", ntrip_interval_s,
  "--rev1gga", ntrip_rev1gga_s,
  NULL,
};

static char *ntrip_argv_username_debug[] = {
  "ntrip_daemon",
  "--debug",
  "--file", FIFO_FILE_PATH,
  "--username", ntrip_username,
  "--password", ntrip_password,
  "--url", ntrip_url,
  "--interval", ntrip_interval_s,
  "--rev1gga", ntrip_rev1gga_s,
  NULL,
};
// clang-format on

static char **ntrip_argv = ntrip_argv_normal;

typedef struct {
  int (*execfn)(void);
  int pid;
} ntrip_process_t;

static int ntrip_daemon_execfn(void)
{
  return execvp(ntrip_argv[0], ntrip_argv);
}

static int ntrip_adapter_execfn(void)
{
  // clang-format off
  char *argv[] = {
    "endpoint_adapter",
    "--name", "ntrip_daemon",
    "-f", "rtcm3",
    "--file", FIFO_FILE_PATH,
    "-p", "ipc:///var/run/sockets/rtcm3_external.sub",
    NULL,
  };
  // clang-format on

  return execvp(argv[0], argv);
}

static ntrip_process_t ntrip_processes[] = {
  {.pid = 0, .execfn = ntrip_adapter_execfn},
  {.pid = 0, .execfn = ntrip_daemon_execfn},
};

static const size_t ntrip_processes_count = COUNT_OF(ntrip_processes);

static void ntrip_stop_process(size_t i)
{
  ntrip_process_t *process = &ntrip_processes[i];
  pid_t process_pid = process->pid;
  if (process_pid != 0) {
    piksi_log(LOG_DEBUG, "%s: senging SIGTERM to pid %d", __FUNCTION__, process_pid);
    int ret = kill(process_pid, SIGTERM);
    if (ret != 0) {
      piksi_log(LOG_ERR, "kill pid %d error (%d) \"%s\"", process_pid, errno, strerror(errno));
    }
    sleep(0.5);
    process_pid = process->pid;
    if (process_pid != 0) {
      piksi_log(LOG_WARNING, "%s: senging SIGKILL to pid %d", __FUNCTION__, process_pid);
      ret = kill(process_pid, SIGKILL);
      if (ret != 0 && errno != ESRCH) {
        piksi_log(LOG_ERR,
                  "force kill pid %d error (%d) \"%s\"",
                  process_pid,
                  errno,
                  strerror(errno));
      }
    }
    process->pid = 0;
  }
}

void ntrip_record_exit(pid_t pid)
{
  for (size_t i = 0; i < ntrip_processes_count; i++) {
    ntrip_process_t *process = &ntrip_processes[i];
    if (process->pid != 0 && process->pid == pid) {
      piksi_log(LOG_DEBUG, "known child process pid %d exited", process->pid);
      process->pid = 0;
      return;
    }
  }
}

void ntrip_stop_processes()
{
  for (size_t i = 0; i < ntrip_processes_count; i++) {
    ntrip_stop_process(i);
  }
}

static void ntrip_start_processes()
{
  snprintf(ntrip_interval_s, sizeof(ntrip_interval_s), "%d", ntrip_interval);
  ntrip_rev1gga_s[0] = ntrip_rev1gga ? 'y' : 'n';

  for (size_t i = 0; i < ntrip_processes_count; i++) {
    ntrip_stop_process(i);

    /* TODO: Remove the need for these by reading from control socket */
    if (!ntrip_enabled || strcmp(ntrip_url, "") == 0) {
      system("echo 0 >" NTRIP_ENABLED_FILE_PATH);
      continue;
    }

    system("echo 1 >" NTRIP_ENABLED_FILE_PATH);

    if (strcmp(ntrip_username, "") != 0 && strcmp(ntrip_password, "") != 0) {
      ntrip_argv = ntrip_debug ? ntrip_argv_username_debug : ntrip_argv_username;
    } else {
      ntrip_argv = ntrip_debug ? ntrip_argv_debug : ntrip_argv_normal;
    }

    ntrip_process_t *process = &ntrip_processes[i];

    process->pid = fork();
    if (process->pid == 0) {
      process->execfn();
      piksi_log(LOG_ERR | LOG_SBP, "exec error (%d) \"%s\"", errno, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
}

static int ntrip_notify_generic(void *context)
{
  (void)context;

  /* ntrip_settings_inititalized prevents false warning when notify function
   * is triggered by read from persistent config file during boot */
  if (ntrip_enabled && ntrip_settings_initialized) {
    sbp_log(LOG_WARNING, "NTRIP must be disabled to modify settings");
    return SBP_SETTINGS_WRITE_STATUS_MODIFY_DISABLED;
  }

  return SBP_SETTINGS_WRITE_STATUS_OK;
}

static int ntrip_notify_enable(void *context)
{
  (void)context;

  /* Check if initialization is in process and this notify function was
   * triggered by read from persistent config file during boot. If this is the
   * case, other settings might not be ready yet. */
  if (ntrip_settings_initialized) {
    ntrip_start_processes();
  }

  return 0;
}

void ntrip_settings_init(settings_ctx_t *settings_ctx)
{
  piksi_log(LOG_DEBUG, "ntrip process count %zu", ntrip_processes_count);

  mkfifo(FIFO_FILE_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  // clang-format off
  settings_register(settings_ctx, "ntrip", "enable",
                    &ntrip_enabled, sizeof(ntrip_enabled),
                    SETTINGS_TYPE_BOOL,
                    ntrip_notify_enable, NULL);

  settings_register(settings_ctx, "ntrip", "debug",
                    &ntrip_debug, sizeof(ntrip_debug),
                    SETTINGS_TYPE_BOOL,
                    ntrip_notify_generic, NULL);

  settings_register(settings_ctx, "ntrip", "username",
                    &ntrip_username, sizeof(ntrip_username),
                    SETTINGS_TYPE_STRING,
                    ntrip_notify_generic, NULL);

  settings_register(settings_ctx, "ntrip", "password",
                    &ntrip_password, sizeof(ntrip_password),
                    SETTINGS_TYPE_STRING,
                    ntrip_notify_generic, NULL);

  settings_register(settings_ctx, "ntrip", "url",
                    &ntrip_url, sizeof(ntrip_url),
                    SETTINGS_TYPE_STRING,
                    ntrip_notify_generic, NULL);

  settings_register(settings_ctx, "ntrip", "gga_out_interval",
                    &ntrip_interval, sizeof(ntrip_interval),
                    SETTINGS_TYPE_INT,
                    ntrip_notify_generic, NULL);

  settings_register(settings_ctx, "ntrip", "gga_out_rev1",
                    &ntrip_rev1gga, sizeof(ntrip_rev1gga),
                    SETTINGS_TYPE_BOOL,
                    ntrip_notify_generic, NULL);

  ntrip_settings_initialized = true;
  // clang-format on

  /* Settings ready, start processes accordingly.
   *
   * This explicit call and initialization checking could be avoided if
   * ntrip.enable is registered as the last one in the ntrip settings group.
   * Meaning ntrip.enable reads as false while other settings would be
   * registered and possibly read from the persistent config during boot.
   *
   * This alternative approach has the downside that it will show ntrip.enable
   * as the last setting in the ntrip settings goup on console affecting end
   * user experience. */
  ntrip_start_processes();
}

bool ntrip_reconnect(void)
{

  for (size_t i = 0; i < ntrip_processes_count; i++) {

    ntrip_process_t *process = &ntrip_processes[i];

    if (process->execfn == ntrip_daemon_execfn) {

      if (process->pid == 0) {
        piksi_log(LOG_ERR, "Asked to tell ntrip_daemon to reconnect, but it isn't running");
        return false;
      }

      int ret = kill(process->pid, SIGUSR1);

      if (ret != 0) {
        piksi_log(LOG_ERR,
                  "ntrip_reconnect: kill pid %d error (%d) \"%s\"",
                  process->pid,
                  errno,
                  strerror(errno));

        return false;
      }
    }
  }

  return true;
}
