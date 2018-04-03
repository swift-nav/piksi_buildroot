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

#include <string.h>
#include <assert.h>

#include <libpiksi/runit.h>
#include <libpiksi/logging.h>

#define CHECK_FS_CALL(TheCheck, TheCall, ThePath) if (!(TheCheck)) { \
    piksi_log(LOG_ERR, "%s: %s: %s (error: %s)", __FUNCTION__, TheCall, \
              ThePath, strerror(errno)); \
    return -1; }

#define CHECK_SPRINTF(TheSnprintf) { \
    int count = TheSnprintf; \
    assert((size_t)count < sizeof(service_dir)); }

#define RUN_SYSTEM_CMD(TheCommand) { \
  int rc = system(TheCommand); \
  if(rc != 0) { \
    piksi_log(LOG_ERR, "%s: system: %s (error: %d)", __FUNCTION__, \
              TheCommand, rc);  \
    return -1; } }

int start_runit_service(const char* runit_service_dir,
                        const char* service_name,
                        const char* command_line,
                        bool restart)
{
  static const int control_sleep_us = 100e3;
  static const int max_wait_us = 5e6;

  int control_pipe_retries = max_wait_us / control_sleep_us;
  assert( control_pipe_retries > 0 && control_pipe_retries <= 1000 );

  char service_dir[PATH_MAX];
  char path_buf[PATH_MAX];
  char command_buf[1024];

  CHECK_SPRINTF(snprintf(service_dir, sizeof(service_dir), "%s/%s", runit_service_dir, service_name));
  CHECK_SPRINTF(snprintf(command_buf, sizeof(command_buf), "rm -rf %s", service_dir));

  RUN_SYSTEM_CMD(command_buf);

  CHECK_FS_CALL(mkdir(service_dir, 0755) == 0, "mkdir", service_dir);

  CHECK_SPRINTF(snprintf(path_buf, sizeof(path_buf), "%s/%s", service_dir, "down"));

  FILE* fp = fopen(path_buf, "w");
  CHECK_FS_CALL(fp != NULL, "fopen", path_buf);
  CHECK_FS_CALL(fclose(fp) == 0, "fclose", path_buf);

  CHECK_SPRINTF(snprintf(path_buf, sizeof(path_buf), "%s/%s", service_dir, "run"));

  FILE* run_fp = fopen(path_buf, "w");
  CHECK_FS_CALL(run_fp != NULL, "fopen", path_buf);

  fprintf(run_fp, "#!/bin/sh\n");
  fprintf(run_fp, "exec %s\n", command_line);

  CHECK_FS_CALL(fclose(run_fp) == 0, "fclose", path_buf);
  CHECK_FS_CALL(chmod(path_buf, 0755) == 0, "chmod", path_buf);

  CHECK_SPRINTF(snprintf(path_buf, sizeof(path_buf), "%s/supervise/control", service_dir));

  struct stat s;
  while (stat(path_buf, &s) != 0 && control_pipe_retries-- > 0) {
    assert( usleep(control_sleep_us) == 0 );
  }

  FILE* control_fp = fopen(path_buf, "w");
  CHECK_FS_CALL(control_fp != NULL, "fopen", path_buf);

  if (restart)
    fprintf(control_fp, "u");
  else
    fprintf(control_fp, "o");

  CHECK_FS_CALL(fclose(control_fp) == 0, "fclose", path_buf);

  return 0;
}

int stop_runit_service(const char* runit_service_dir, const char* service_name)
{
  char service_dir[PATH_MAX];
  char path_buf[PATH_MAX];

  CHECK_SPRINTF(snprintf(service_dir,
        sizeof(service_dir), "%s/%s", runit_service_dir, service_name));
  CHECK_SPRINTF(snprintf(path_buf,
        sizeof(path_buf), "%s/supervise/stat", service_dir));

  struct stat s;
  if (stat(path_buf, &s) != 0 && errno == ENOENT) {
    // Service already down
    return 0;
  }

  FILE* stat_fp = fopen(path_buf, "w");
  CHECK_FS_CALL(stat_fp != NULL, "fopen", path_buf);

  char stat_buf[64] = {0};
  (void) fread(stat_buf, sizeof(stat_buf), 1, stat_fp);

  if (strcmp(stat_buf, "down")) {
    CHECK_FS_CALL(fclose(stat_fp) == 0, "fclose", path_buf);
    // Service already down...
    return 0;
  }

  CHECK_FS_CALL(fclose(stat_fp) == 0, "fclose", path_buf);

  CHECK_SPRINTF(snprintf(path_buf, sizeof(path_buf), "%s/supervise/control", service_dir));

  FILE* control_fp = fopen(path_buf, "w");
  CHECK_FS_CALL(control_fp != NULL, "fopen", path_buf);

  // Stop the service
  fprintf(control_fp, "d");

  CHECK_FS_CALL(fclose(control_fp) == 0, "fclose", path_buf);

  char command_buf[1024];
  CHECK_SPRINTF(snprintf(command_buf, sizeof(command_buf), "rm -rf %s", service_dir));

  RUN_SYSTEM_CMD(command_buf);

  return 0;
}

#undef CHECK_FS_CALL
#undef CHECK_SPRINTF
#undef RUN_SYSTEM_CMD
