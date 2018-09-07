/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
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
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libpiksi/util.h>
#include <libpiksi/logging.h>

#define SBP_SENDER_ID_FILE_PATH "/cfg/sbp_sender_id"
#define DEVICE_UUID_FILE_PATH   "/cfg/device_uuid"

#define DEVICE_DURO_EEPROM_PATH "/cfg/duro_eeprom"
#define DEVICE_DURO_MAX_CONTENTS_SIZE (128u)
#define DEVICE_DURO_ID_STRING "DUROV0"

#define PROC_UPTIME_FILE_PATH   "/proc/uptime"
#define UPTIME_READ_MAX_LENGTH (64u)

#define GPS_TIME_FILE_PATH "/var/run/health/gps_time_available"

static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    piksi_log(LOG_ERR, "error opening %s", filename);
    return -1;
  }

  bool success = (fgets(str, str_size, fp) != NULL);

  fclose(fp);

  if (!success) {
    piksi_log(LOG_ERR, "error reading %s", filename);
    return -1;
  }

  return 0;
}

u16 sbp_sender_id_get(void)
{
  u16 sbp_sender_id = SBP_SENDER_ID;

  char sbp_sender_id_string[32];
  if (file_read_string(SBP_SENDER_ID_FILE_PATH, sbp_sender_id_string,
                       sizeof(sbp_sender_id_string)) == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }

  return sbp_sender_id;
}

u64 system_uptime_ms_get(void)
{
  char uptime_string[UPTIME_READ_MAX_LENGTH];
  u64 uptime_ms = 0;
  if (file_read_string(PROC_UPTIME_FILE_PATH, uptime_string,
                       sizeof(uptime_string)) == 0) {
    uptime_ms = (u64)(1e3 * strtod(uptime_string, NULL));
  }
  return uptime_ms;
}

int device_uuid_get(char *str, size_t str_size)
{
  return file_read_string(DEVICE_UUID_FILE_PATH, str, str_size);
}

bool device_is_duro(void)
{
  char duro_eeprom_sig[sizeof(DEVICE_DURO_ID_STRING)];

  int fd = open(DEVICE_DURO_EEPROM_PATH, O_RDONLY);
  if (fd < 0) {
    piksi_log(LOG_WARNING, "Failed to open DURO eeprom path");
    return false;
  }
  read(fd, duro_eeprom_sig, sizeof(DEVICE_DURO_ID_STRING));
  close(fd);

  return (memcmp(duro_eeprom_sig,
                 DEVICE_DURO_ID_STRING,
                 strlen(DEVICE_DURO_ID_STRING)) == 0);
}

void set_device_has_gps_time(bool has_time) {
  static bool had_time = false;
  bool file_updated = false;

  /* React only if the state changes */
  if (has_time != had_time) {
    FILE* fp = fopen(GPS_TIME_FILE_PATH, "w");

    if (fp == NULL) {
      piksi_log(LOG_WARNING|LOG_SBP,
                "Failed to open %s: errno = %d",
                GPS_TIME_FILE_PATH,
                errno);
      return;
    }

    char buffer[] = { (has_time ? '1' : '0'), '\n' };

    if (fwrite(buffer, sizeof(char), sizeof(buffer), fp) != sizeof(buffer)) {
      piksi_log(LOG_WARNING|LOG_SBP, "Failed to write %s", GPS_TIME_FILE_PATH);
    } else {
      file_updated = true;
    }

    fclose(fp);
  }

  /* Keep the file and local variable in sync */
  if (file_updated) {
    had_time = has_time;
    piksi_log(LOG_DEBUG|LOG_SBP,
              "%s updated with value %u",
              GPS_TIME_FILE_PATH,
              has_time);
  }
}

bool device_has_gps_time(void) {
  bool has_time = false;

  if (access(GPS_TIME_FILE_PATH, F_OK) == -1) {
    /* File is not created yet, system is most likely still booting */
    piksi_log(LOG_DEBUG, "%s doesn't exist", GPS_TIME_FILE_PATH);
    return has_time;
  }

  FILE* fp = fopen(GPS_TIME_FILE_PATH, "r");

  if (fp == NULL) {
    piksi_log(LOG_WARNING|LOG_SBP,
              "Failed to open %s: errno = %d",
              GPS_TIME_FILE_PATH,
              errno);
    return has_time;
  }

  has_time = ('1' == fgetc(fp));

  fclose(fp);

  return has_time;
}

void reap_children(bool debug, child_exit_fn_t exit_handler)
{
  int errno_saved = errno;
  int status;

  pid_t child_pid;

  while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (WIFEXITED(status)) {
      if (exit_handler != NULL) exit_handler(child_pid);
      if (debug) {
        piksi_log(LOG_DEBUG, "%s: child (pid: %d) exit status: %d",
                  __FUNCTION__, child_pid, WEXITSTATUS(status));
      }
    } else if (WIFSIGNALED(status)) {
      if (exit_handler != NULL) exit_handler(child_pid);
      if (debug) {
        piksi_log(LOG_DEBUG, "%s: child (pid: %d) term signal: %d",
                  __FUNCTION__, child_pid, WTERMSIG(status));
      }
    } else if (WIFSTOPPED(status)) {
      if (debug) {
        piksi_log(LOG_DEBUG, "%s: child (pid: %d) stop signal: %d",
                  __FUNCTION__, child_pid, WSTOPSIG(status));
      }
    } else {
      if (debug) {
        piksi_log(LOG_DEBUG, "%s: child (pid: %d) unknown status: %d",
                  __FUNCTION__, child_pid, status);
      }
    }
  }

  errno = errno_saved;
}

void setup_sigchld_handler(void (*handler)(int))
{
  struct sigaction sigchild_sa;

  sigchild_sa.sa_handler = handler;
  sigemptyset(&sigchild_sa.sa_mask);
  sigchild_sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  if ((sigaction(SIGCHLD, &sigchild_sa, NULL) != 0))
  {
    piksi_log(LOG_ERR, "error setting up SIGCHLD handler");
    exit(-1);
  }
}

void setup_sigint_handler(void (*handler)(int signum, siginfo_t *info, void *ucontext))
{
  struct sigaction interrupt_sa;

  interrupt_sa.sa_sigaction = handler;
  sigemptyset(&interrupt_sa.sa_mask);
  interrupt_sa.sa_flags = SA_SIGINFO;

  if ((sigaction(SIGINT, &interrupt_sa, NULL) != 0))
  {
    piksi_log(LOG_ERR, "error setting up SIGINT handler");
    exit(-1);
  }
}

void setup_sigterm_handler(void (*handler)(int signum, siginfo_t *info, void *ucontext))
{
  struct sigaction terminate_sa;

  terminate_sa.sa_sigaction = handler;
  sigemptyset(&terminate_sa.sa_mask);
  terminate_sa.sa_flags = SA_SIGINFO;

  if ((sigaction(SIGTERM, &terminate_sa, NULL) != 0))
  {
    piksi_log(LOG_ERR, "error setting up SIGTERM handler");
    exit(-1);
  }
}
