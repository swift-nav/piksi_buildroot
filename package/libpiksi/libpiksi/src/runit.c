/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include <libpiksi/runit.h>
#include <libpiksi/logging.h>

//// clang-format off
//#define DEBUG_RUNIT
#ifdef DEBUG_RUNIT
#define RUNIT_DEBUG_LOG(ThePattern, ...) \
  piksi_log(LOG_DEBUG, ("%s: " ThePattern), __FUNCTION__, ##__VA_ARGS__)
#define NDEBUG_UNUSED(X)
#else
#define RUNIT_DEBUG_LOG(...)
#define NDEBUG_UNUSED(X) (void)(X)
#endif

// Runit status strings
#define RUNIT_STAT_DOWN "down\n"
#define RUNIT_STAT_RUNNING "run\n"
#define RUNIT_STAT_RUNNING_WANT_DOWN "run, want down\n"
#define RUNIT_STAT_EMPTY "\n"

#define CHECK_FS_CALL(TheCheck, TheCall, ThePath)                                                  \
  if (!(TheCheck)) {                                                                               \
    piksi_log(LOG_ERR, "%s: %s: %s (error: %s)", __FUNCTION__, TheCall, ThePath, strerror(errno)); \
    return -1;                                                                                     \
  }

#define CHECKED_SPRINTF(TheDest, TheSize, ThePattern, ...)             \
  {                                                                    \
    int count = snprintf(TheDest, TheSize, ThePattern, ##__VA_ARGS__); \
    assert((size_t)count < TheSize);                                   \
  }
// clang-format on

int start_runit_service(runit_config_t *cfg)
{
  static const int control_sleep_us = 100e3;
  static const int max_wait_us = 6e6;

  struct stat s;

  int control_pipe_retries = max_wait_us / control_sleep_us;
  assert(control_pipe_retries > 0 && control_pipe_retries <= 1000);

  char tmp_service_dir[PATH_MAX];
  strncpy(tmp_service_dir, "/tmp/tmp.runit.XXXXXX", sizeof(tmp_service_dir));

  char service_dir_final[PATH_MAX];
  CHECKED_SPRINTF(service_dir_final,
                  sizeof(service_dir_final),
                  "%s/%s",
                  cfg->service_dir,
                  cfg->service_name);

  char *service_dir = NULL;

  if (stat(service_dir_final, &s) == 0) {
    service_dir = service_dir_final;
    RUNIT_DEBUG_LOG("service dir (%s) already exists", service_dir);
  } else {
    service_dir = mkdtemp(tmp_service_dir);
    CHECK_FS_CALL(service_dir != NULL, "mkdtemp", tmp_service_dir);
  }

  char path_buf[PATH_MAX];

  if (cfg->custom_down != NULL) {
    CHECKED_SPRINTF(path_buf, sizeof(path_buf), "%s/%s", service_dir, "control");
    if (stat(path_buf, &s) == 0) {
      RUNIT_DEBUG_LOG("control dir (%s) already exists", path_buf);
    } else {
      CHECK_FS_CALL(mkdir(path_buf, 0755) == 0, "mkdir", service_dir);
    }
  }

  // Write the <service>/down file so the service doesn't start automatically
  CHECKED_SPRINTF(path_buf, sizeof(path_buf), "%s/%s", service_dir, "down");
  FILE *fp = fopen(path_buf, "w");
  CHECK_FS_CALL(fp != NULL, "fopen", path_buf);
  CHECK_FS_CALL(fclose(fp) == 0, "fclose", path_buf);

  CHECKED_SPRINTF(path_buf, sizeof(path_buf), "%s/%s", service_dir, "run");

  FILE *run_fp = fopen(path_buf, "w");
  CHECK_FS_CALL(run_fp != NULL, "fopen", path_buf);

  fprintf(run_fp, "#!/bin/sh\n");
  fprintf(run_fp, "exec %s\n", cfg->command_line);

  CHECK_FS_CALL(fclose(run_fp) == 0, "fclose", path_buf);
  CHECK_FS_CALL(chmod(path_buf, 0755) == 0, "chmod", path_buf);

  if (cfg->custom_down != NULL) {

    CHECKED_SPRINTF(path_buf, sizeof(path_buf), "%s/%s", service_dir, "control/d");

    FILE *custom_down_fp = fopen(path_buf, "w");
    CHECK_FS_CALL(custom_down_fp != NULL, "fopen", path_buf);

    fprintf(custom_down_fp, "#!/bin/sh\n");
    fprintf(custom_down_fp, "exec %s\n", cfg->custom_down);

    CHECK_FS_CALL(fclose(custom_down_fp) == 0, "fclose", path_buf);
    CHECK_FS_CALL(chmod(path_buf, 0755) == 0, "chmod", path_buf);
  }

  if (cfg->finish_command != NULL) {

    CHECKED_SPRINTF(path_buf, sizeof(path_buf), "%s/%s", service_dir, "finish");

    FILE *finish_fp = fopen(path_buf, "w");
    CHECK_FS_CALL(finish_fp != NULL, "fopen", path_buf);

    fprintf(finish_fp, "#!/bin/sh\n");
    fprintf(finish_fp, "exec %s\n", cfg->finish_command);

    CHECK_FS_CALL(fclose(finish_fp) == 0, "fclose", path_buf);
    CHECK_FS_CALL(chmod(path_buf, 0755) == 0, "chmod", path_buf);
  }

  if (service_dir_final != service_dir) {
    char rename_command[1024];
    CHECKED_SPRINTF(rename_command,
                    sizeof(rename_command),
                    "/bin/mv %s %s",
                    service_dir,
                    service_dir_final);
    RUNIT_DEBUG_LOG("Moving service dir into place: %s", rename_command);
    CHECK_FS_CALL(system(rename_command) == 0, "system", rename_command);
  }

  CHECKED_SPRINTF(path_buf, sizeof(path_buf), "%s/supervise/control", service_dir_final);

  RUNIT_DEBUG_LOG("Waiting for control socket (%s) ...", path_buf);
  while (stat(path_buf, &s) != 0 && control_pipe_retries-- > 0) {
    assert(usleep(control_sleep_us) == 0);
  }
  RUNIT_DEBUG_LOG("Found control socket (%s) ...", path_buf);

  FILE *control_fp = fopen(path_buf, "w");
  CHECK_FS_CALL(control_fp != NULL, "fopen", path_buf);

  char sv_start_command[1024];

  if (cfg->restart) {
    fprintf(control_fp, "u");
    CHECKED_SPRINTF(sv_start_command,
                    sizeof(sv_start_command),
                    "/usr/bin/sv up %s",
                    service_dir_final);
  } else {
    fprintf(control_fp, "o");
    CHECKED_SPRINTF(sv_start_command,
                    sizeof(sv_start_command),
                    "/usr/bin/sv once %s",
                    service_dir_final);
  }

  CHECK_FS_CALL(system(sv_start_command) == 0, "system", sv_start_command);
  CHECK_FS_CALL(fclose(control_fp) == 0, "fclose", path_buf);

  return 0;
}

runit_stat_t stat_runit_service(runit_config_t *cfg)
{
  char service_dir[PATH_MAX];
  char path_buf[PATH_MAX];

  size_t read_count;
  NDEBUG_UNUSED(read_count);

  struct stat s;

  CHECKED_SPRINTF(service_dir, sizeof(service_dir), "%s/%s", cfg->service_dir, cfg->service_name);

  CHECKED_SPRINTF(path_buf, sizeof(path_buf), "%s/supervise/stat", service_dir);

  if (stat(path_buf, &s) != 0 && errno == ENOENT) {
    return RUNIT_NO_STAT;
  }

  FILE *stat_fp = fopen(path_buf, "r");
  CHECK_FS_CALL(stat_fp != NULL, "fopen", path_buf);

  char stat_buf[64] = {0};
  read_count = fread(stat_buf, 1, sizeof(stat_buf), stat_fp);

  RUNIT_DEBUG_LOG("service status: '%s' (read_count: %llu)", stat_buf, read_count);


  if (strcmp(stat_buf, RUNIT_STAT_DOWN) == 0) {
    CHECK_FS_CALL(fclose(stat_fp) == 0, "fclose", path_buf);
    return RUNIT_DOWN;
  }

  if (strcmp(stat_buf, RUNIT_STAT_RUNNING) == 0
      || strcmp(stat_buf, RUNIT_STAT_RUNNING_WANT_DOWN) == 0) {
    CHECK_FS_CALL(fclose(stat_fp) == 0, "fclose", path_buf);
    return RUNIT_RUNNING;
  }

  if (strcmp(stat_buf, RUNIT_STAT_EMPTY) == 0) {

    char pid_path_buf[PATH_MAX];
    CHECKED_SPRINTF(pid_path_buf, sizeof(pid_path_buf), "%s/supervise/pid", service_dir);

    struct stat s_pid;
    if (stat(pid_path_buf, &s_pid) != 0) {
      CHECK_FS_CALL(fclose(stat_fp) == 0, "fclose", path_buf);
      return RUNIT_NO_PID;
    }

    FILE *pid_fp = fopen(pid_path_buf, "r");
    CHECK_FS_CALL(pid_fp != NULL, "fopen", pid_path_buf);

    char pid_buf[64] = {0};
    read_count = fread(pid_buf, 1, sizeof(pid_buf), pid_fp);

    RUNIT_DEBUG_LOG("service status, pid: '%s' (read_count: %llu)", pid_buf, read_count);

    CHECK_FS_CALL(fclose(pid_fp) == 0, "fclose", pid_path_buf);

    if (strcmp(pid_buf, RUNIT_STAT_EMPTY) != 0) {
      CHECK_FS_CALL(fclose(stat_fp) == 0, "fclose", path_buf);
      return RUNIT_RUNNING;
    }
  }

  if (strstr(stat_buf, RUNIT_STAT_RUNNING) == stat_buf) {
    CHECK_FS_CALL(fclose(stat_fp) == 0, "fclose", path_buf);
    return RUNIT_RUNNING_OTHER;
  }

  CHECK_FS_CALL(fclose(stat_fp) == 0, "fclose", path_buf);
  piksi_log(LOG_DEBUG,
            "%s: returning status unknown even though stat file existed: %s",
            __FUNCTION__,
            stat_buf);

  return RUNIT_UNKNOWN;
}

const char *runit_status_str(runit_stat_t status)
{
  switch (status) {
  case RUNIT_UNKNOWN: return "unknown";
  case RUNIT_RUNNING_OTHER: return "running (other)";
  case RUNIT_RUNNING: return "running";
  case RUNIT_DOWN: return "down";
  case RUNIT_NO_STAT: return "no stat";
  case RUNIT_NO_PID: return "no pid file";
  }

  return "?";
}

int stop_runit_service(runit_config_t *cfg)
{
  runit_stat_t stat = stat_runit_service(cfg);

  if (stat != RUNIT_RUNNING) {
    piksi_log(LOG_WARNING,
              "service %s reported status other than running: %s",
              cfg->service_name,
              runit_status_str(stat));
    return -1;
  }

  RUNIT_DEBUG_LOG("service %s reported status: %s", cfg->service_name, runit_status_str(stat));

  char service_dir[PATH_MAX];
  CHECKED_SPRINTF(service_dir, sizeof(service_dir), "%s/%s", cfg->service_dir, cfg->service_name);

  char path_buf[PATH_MAX];
  CHECKED_SPRINTF(path_buf, sizeof(path_buf), "%s/supervise/control", service_dir);

  FILE *control_fp = fopen(path_buf, "w");
  CHECK_FS_CALL(control_fp != NULL, "fopen", path_buf);

  // Stop the service
  piksi_log(LOG_DEBUG, "sending stop command to service %s", cfg->service_name);
  fprintf(control_fp, "d");

  CHECK_FS_CALL(fclose(control_fp) == 0, "fclose", path_buf);

  return 0;
}

#undef CHECK_FS_CALL
#undef CHECK_SPRINTF
#undef RUNIT_DEBUG_LOG
#undef NDEBUG_UNUSED
