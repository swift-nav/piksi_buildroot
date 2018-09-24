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

/* from pfwp init.c */
#define IMAGE_HARDWARE_INVALID 0xffffffff
#define IMAGE_HARDWARE_UNKNOWN 0x00000000
#define IMAGE_HARDWARE_V3_MICROZED 0x00000001
#define IMAGE_HARDWARE_V3_EVT1 0x00000011
#define IMAGE_HARDWARE_V3_EVT2 0x00000012
#define IMAGE_HARDWARE_V3_PROD 0x00000013

#define SBP_SENDER_ID_FILE_PATH "/cfg/sbp_sender_id"
#define DEVICE_UUID_FILE_PATH "/cfg/device_uuid"

#define DEVICE_HARDWARE_REVISION_FILE_PATH "/factory/hardware"
#define DEVICE_HARDWARE_REVISION_MAX_LENGTH (64u)
#define DEVICE_HARDWARE_VERSION_FILE_PATH "/factory/hardware_version"
#define DEVICE_HARDWARE_VERSION_MAX_LENGTH (64u)

#define DEVICE_DURO_EEPROM_PATH "/cfg/duro_eeprom"
#define DEVICE_DURO_MAX_CONTENTS_SIZE (128u)
#define DEVICE_DURO_ID_STRING "DUROV0"
#define POSEDAEMON_FILE_PATH "/usr/bin/PoseDaemon"
#define SMOOTHPOSE_LICENSE_FILE_PATH "/persistent/licenses/smoothpose_license.json"
#define DEVICE_DURO_EEPROM_RETRY_INTERVAL_MS 250
#define DEVICE_DURO_EEPROM_RETRY_TIMES 6

#define PROC_UPTIME_FILE_PATH "/proc/uptime"
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

bool file_read_value(char *file_path)
{
  /* Accommodate also the terminating null char fgets always adds */
  char val_char[2];
  if (file_read_string(file_path, val_char, sizeof(val_char)) != 0) {
    return false;
  }

  return ('1' == val_char[0]);
}

u16 sbp_sender_id_get(void)
{
  u16 sbp_sender_id = SBP_SENDER_ID;

  char sbp_sender_id_string[32];
  if (file_read_string(SBP_SENDER_ID_FILE_PATH, sbp_sender_id_string, sizeof(sbp_sender_id_string))
      == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }

  return sbp_sender_id;
}

u64 system_uptime_ms_get(void)
{
  char uptime_string[UPTIME_READ_MAX_LENGTH];
  u64 uptime_ms = 0;
  if (file_read_string(PROC_UPTIME_FILE_PATH, uptime_string, sizeof(uptime_string)) == 0) {
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
  char duro_eeprom_sig[DEVICE_DURO_MAX_CONTENTS_SIZE];
  /* DEVICE_DURO_EEPROM_PATH will be created by S18 whether
   * there is EEPROM or not */
  for (int i; i < DEVICE_DURO_EEPROM_RETRY_TIMES; i++) {
    if (access(DEVICE_DURO_EEPROM_PATH, F_OK) == 0) {
      break;
    }
    usleep(DEVICE_DURO_EEPROM_RETRY_INTERVAL_MS * 1000);
  }
  if (file_read_string(DEVICE_DURO_EEPROM_PATH, duro_eeprom_sig, sizeof(duro_eeprom_sig)) != 0) {
    piksi_log(LOG_WARNING, "Failed to read DURO eeprom contents");
    return false;
  }

  return (memcmp(duro_eeprom_sig, DEVICE_DURO_ID_STRING, strlen(DEVICE_DURO_ID_STRING)) == 0);
}

static bool device_has_ins(void)
{
  return (access(POSEDAEMON_FILE_PATH, F_OK) != -1
          && access(SMOOTHPOSE_LICENSE_FILE_PATH, F_OK) != -1);
}

int hw_version_string_get(char *hw_version_string, size_t size)
{
  char raw_hw_ver_string[DEVICE_HARDWARE_VERSION_MAX_LENGTH];
  u64 hw_version = 0;
  if (file_read_string(DEVICE_HARDWARE_VERSION_FILE_PATH,
                       raw_hw_ver_string,
                       sizeof(raw_hw_ver_string))
      == 0) {
    hw_version = (u64)strtod(raw_hw_ver_string, NULL);
    if (hw_version == 0 && errno == EINVAL) {
      piksi_log(LOG_ERR, "Error converting hardware version string: EINVAL");
      return -1;
    }
  } else {
    piksi_log(LOG_ERR,
              "Error reading hardware version string (buffer size: %d)",
              sizeof(raw_hw_ver_string));
    return -1;
  }
  u16 major_ver = hw_version >> 16;
  u16 minor_ver = hw_version & 0xFFFF;
  int written = snprintf(hw_version_string, size, "%d.%d", major_ver, minor_ver);
  if (written < 0) {
    piksi_log(LOG_ERR, "Error writing hardware version string to buffer");
    return -1;
  }
  if (written >= size) {
    piksi_log(
      LOG_ERR,
      "Hardware version string truncated when writing to buffer (size of intended string: %d)",
      written);
    return -1;
  }
  return 0;
}

int hw_revision_string_get(char *hw_revision_string, size_t size)
{
  const char *s = NULL;

  char raw_hw_rev_string[DEVICE_HARDWARE_REVISION_MAX_LENGTH];
  u16 hw_revision = 0;
  if (file_read_string(DEVICE_HARDWARE_REVISION_FILE_PATH,
                       raw_hw_rev_string,
                       sizeof(raw_hw_rev_string))
      == 0) {
    hw_revision = (u16)strtod(raw_hw_rev_string, NULL);
    if (hw_revision == 0 && errno == EINVAL) {
      piksi_log(LOG_ERR, "Error converting hardware revision string: EINVAL");
      return -1;
    }
  } else {
    piksi_log(LOG_ERR,
              "Error reading hardware revision string (buffer size: %d)",
              sizeof(raw_hw_rev_string));
    return -1;
  }

  switch (hw_revision) {
  case IMAGE_HARDWARE_UNKNOWN: s = "Unknown"; break;
  case IMAGE_HARDWARE_V3_MICROZED: s = "Piksi Multi MicroZed"; break;
  case IMAGE_HARDWARE_V3_EVT1: s = "Piksi Multi EVT1"; break;
  case IMAGE_HARDWARE_V3_EVT2: s = "Piksi Multi EVT2"; break;
  case IMAGE_HARDWARE_V3_PROD: s = "Piksi Multi"; break;
  default: s = "Invalid"; break;
  }

  if (strlen(s) >= size) {
    piksi_log(LOG_ERR,
              "Hardware revision string too large for buffer (size of intended string: %d)",
              strlen(s));
    return -1;
  }
  strncpy(hw_revision_string, s, size);
  return 0;
}

int hw_variant_string_get(char *hw_variant_string, size_t size)
{
  const char *s = NULL;

  if (device_is_duro()) {
    if (device_has_ins()) {
      s = "Duro Inertial";
    } else {
      s = "Duro";
    }
  } else {
    s = "Multi";
  }

  if (strlen(s) >= size) {
    piksi_log(LOG_ERR,
              "Hardware variant string too large for buffer (size of intended string: %d)",
              strlen(s));
    return -1;
  }
  strncpy(hw_variant_string, s, size);
  return 0;
}

int product_id_string_get(char *product_id_string, size_t size)
{
  const char *s = NULL;

  int written = snprintf(product_id_string,
                         size,
                         "%s%s",
                         device_is_duro() ? "Duro" : "Piksi Multi",
                         device_has_ins() ? " Inertial" : "");
  if (written < 0) {
    piksi_log(LOG_ERR, "Error writing product id string to buffer");
    return -1;
  }
  if (written >= size) {
    piksi_log(LOG_ERR,
              "Product id string truncated when writing to buffer (size of intended string: %d)",
              written);
    return -1;
  }
  return 0;
}

void set_device_has_gps_time(bool has_time)
{
  static bool had_time = false;
  bool file_updated = false;

  /* React only if the state changes */
  if (has_time != had_time) {
    FILE *fp = fopen(GPS_TIME_FILE_PATH, "w");

    if (fp == NULL) {
      piksi_log(LOG_WARNING | LOG_SBP, "Failed to open %s: errno = %d", GPS_TIME_FILE_PATH, errno);
      return;
    }

    char buffer[] = {(has_time ? '1' : '0'), '\n'};

    if (fwrite(buffer, sizeof(char), sizeof(buffer), fp) != sizeof(buffer)) {
      piksi_log(LOG_WARNING | LOG_SBP, "Failed to write %s", GPS_TIME_FILE_PATH);
    } else {
      file_updated = true;
    }

    fclose(fp);
  }

  /* Keep the file and local variable in sync */
  if (file_updated) {
    had_time = has_time;
    piksi_log(LOG_DEBUG | LOG_SBP, "%s updated with value %u", GPS_TIME_FILE_PATH, has_time);
  }
}

bool device_has_gps_time(void)
{
  if (access(GPS_TIME_FILE_PATH, F_OK) == -1) {
    /* File is not created yet, system is most likely still booting */
    piksi_log(LOG_DEBUG, "%s doesn't exist", GPS_TIME_FILE_PATH);
    return false;
  }

  return file_read_value(GPS_TIME_FILE_PATH);
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
        piksi_log(LOG_DEBUG,
                  "%s: child (pid: %d) exit status: %d",
                  __FUNCTION__,
                  child_pid,
                  WEXITSTATUS(status));
      }
    } else if (WIFSIGNALED(status)) {
      if (exit_handler != NULL) exit_handler(child_pid);
      if (debug) {
        piksi_log(LOG_DEBUG,
                  "%s: child (pid: %d) term signal: %d",
                  __FUNCTION__,
                  child_pid,
                  WTERMSIG(status));
      }
    } else if (WIFSTOPPED(status)) {
      if (debug) {
        piksi_log(LOG_DEBUG,
                  "%s: child (pid: %d) stop signal: %d",
                  __FUNCTION__,
                  child_pid,
                  WSTOPSIG(status));
      }
    } else {
      if (debug) {
        piksi_log(LOG_DEBUG,
                  "%s: child (pid: %d) unknown status: %d",
                  __FUNCTION__,
                  child_pid,
                  status);
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

  if ((sigaction(SIGCHLD, &sigchild_sa, NULL) != 0)) {
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

  if ((sigaction(SIGINT, &interrupt_sa, NULL) != 0)) {
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

  if ((sigaction(SIGTERM, &terminate_sa, NULL) != 0)) {
    piksi_log(LOG_ERR, "error setting up SIGTERM handler");
    exit(-1);
  }
}
