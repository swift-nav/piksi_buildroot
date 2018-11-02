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
#include <libpiksi/runit.h>
#include <libpiksi/settings_client.h>
#include <libpiksi/util.h>

#include "skylark_settings.h"

// clang-format off
#define SL_RUNIT_SERVICE_DIR            "/var/run/skylark_"
#define SL_HTTP2_RUNIT_SERVICE_NAME     "http2"
#define SL_HTTP1_DL_RUNIT_SERVICE_NAME  "http1_dl"
#define SL_HTTP1_UL_RUNIT_SERVICE_NAME  "http1_ul"
#define SL_ADAPT_DL_RUNIT_SERVICE_NAME  "adapt_dl"
#define SL_ADAPT_UL_RUNIT_SERVICE_NAME  "adapt_ul"

#define UPLOAD_FIFO_FILE_PATH           "/var/run/skylark/upload"
#define DOWNLOAD_FIFO_FILE_PATH         "/var/run/skylark/download"
#define SKYLARK_URL                     "https://broker.skylark.swiftnav.com"
// clang-format on

static const char *const skylark_mode_enum_names[] = {"Disabled", "HTTP 1.1", "HTTP 2", NULL};

enum {
  SKYLARK_MODE_DISABLED,
  SKYLARK_MODE_HTTP_1_1,
  SKYLARK_MODE_HTTP_2,
};

static u8 skylark_enabled;
static char skylark_url[256];

typedef struct {
  runit_config_t cfg;
  u32 modes;
  char *base_cmd;
} skylark_process_t;

static skylark_process_t procs[] = {
  {.cfg =
     {
       .service_dir = SL_RUNIT_SERVICE_DIR SL_ADAPT_DL_RUNIT_SERVICE_NAME "/sv",
       .service_name = SL_ADAPT_DL_RUNIT_SERVICE_NAME,
       .command_line = NULL,
       .custom_down = NULL,
       .restart = NULL,
     },
   .modes = (1 << SKYLARK_MODE_HTTP_1_1) | (1 << SKYLARK_MODE_HTTP_2),
   .base_cmd = "endpoint_adapter --name skylark_download -f sbp --file " DOWNLOAD_FIFO_FILE_PATH
               " -p ipc:///var/run/sockets/skylark.sub"},
  {.cfg =
     {
       .service_dir = SL_RUNIT_SERVICE_DIR SL_ADAPT_UL_RUNIT_SERVICE_NAME "/sv",
       .service_name = SL_ADAPT_UL_RUNIT_SERVICE_NAME,
       .command_line = NULL,
       .custom_down = NULL,
       .restart = NULL,
     },
   .modes = (1 << SKYLARK_MODE_HTTP_1_1) | (1 << SKYLARK_MODE_HTTP_2),
   .base_cmd = "endpoint_adapter --name skylark_upload --file " UPLOAD_FIFO_FILE_PATH
               " -s ipc:///var/run/sockets/skylark.pub "
               "--filter-out sbp --filter-out-config /etc/skylark_upload_filter_out_config"},
  {.cfg =
     {
       .service_dir = SL_RUNIT_SERVICE_DIR SL_HTTP1_DL_RUNIT_SERVICE_NAME "/sv",
       .service_name = SL_HTTP1_DL_RUNIT_SERVICE_NAME,
       .command_line = NULL,
       .custom_down = NULL,
       .restart = NULL,
     },
   .modes = (1 << SKYLARK_MODE_HTTP_1_1),
   .base_cmd = "skylark_daemon --download --file-down " DOWNLOAD_FIFO_FILE_PATH " --url "},
  {.cfg =
     {
       .service_dir = SL_RUNIT_SERVICE_DIR SL_HTTP1_UL_RUNIT_SERVICE_NAME "/sv",
       .service_name = SL_HTTP1_UL_RUNIT_SERVICE_NAME,
       .command_line = NULL,
       .custom_down = NULL,
       .restart = NULL,
     },
   .modes = (1 << SKYLARK_MODE_HTTP_1_1),
   .base_cmd =
     "skylark_daemon --upload --no-error-reporting --file-up " UPLOAD_FIFO_FILE_PATH " --url "},
  {.cfg =
     {
       .service_dir = SL_RUNIT_SERVICE_DIR SL_HTTP2_RUNIT_SERVICE_NAME "/sv",
       .service_name = SL_HTTP2_RUNIT_SERVICE_NAME,
       .command_line = NULL,
       .custom_down = NULL,
       .restart = NULL,
     },
   .modes = (1 << SKYLARK_MODE_HTTP_2),
   .base_cmd = "skylark_daemon --debug --http2 --file-down " DOWNLOAD_FIFO_FILE_PATH
               " --file-up " UPLOAD_FIFO_FILE_PATH " --url "},
};

static char *get_skylark_url(void)
{
  return strcmp(skylark_url, "") == 0 ? SKYLARK_URL : skylark_url;
}

static bool skylark_proc_is_endpoint(const skylark_process_t *proc)
{
  u32 mode = (1 << SKYLARK_MODE_HTTP_1_1) | (1 << SKYLARK_MODE_HTTP_2);
  return (proc->modes == mode);
}

static void skylark_stop_process(skylark_process_t *proc)
{
  if (stat_runit_service(&proc->cfg) != RUNIT_RUNNING) {
    return;
  }

  if (stop_runit_service(&proc->cfg)) {
    piksi_log(LOG_ERR, "stop_runit_service failed for %s", proc->cfg.service_name);
  }
}

void skylark_stop_processes(void)
{
  for (size_t i = 0; i < COUNT_OF(procs); i++) {
    skylark_stop_process(&procs[i]);
  }
}

static int skylark_notify(void *context)
{
  (void)context;

  assert(skylark_enabled <= 2);

  if (0 == skylark_enabled) {
    system("echo 0 >/var/run/skylark/enabled");
  } else {
    system("echo 1 >/var/run/skylark/enabled");
  }

  for (size_t i = 0; i < COUNT_OF(procs); i++) {
    skylark_process_t *process = &procs[i];
    skylark_stop_process(process);

    if (0 == skylark_enabled) {
      continue;
    }

    if (((1 << skylark_enabled) & process->modes) == 0) {
      /* Process is not needed in this mode */
      continue;
    }

    char *cmd = process->base_cmd;
    char *url = "";

    if (!skylark_proc_is_endpoint(process)) {
      url = get_skylark_url();
    }

    char cmd_url[strlen(cmd) + strlen(url) + 1];
    snprintf_assert(cmd_url, sizeof(cmd_url), "%s%s", cmd, url);

    process->cfg.command_line = cmd_url;

    if (start_runit_service(&process->cfg)) {
      piksi_log(LOG_ERR, "start_runit_service failed for %s", process->cfg.service_name);
      return 1;
    }

    /* Clear the pointer because it points to local char array */
    process->cfg.command_line = NULL;
  }

  return SETTINGS_WR_OK;
}

bool skylark_reconnect_dl(void)
{
  for (size_t i = 0; i < COUNT_OF(procs); i++) {

    skylark_process_t *process = &procs[i];

    if (strcmp(process->cfg.service_name, SL_HTTP1_DL_RUNIT_SERVICE_NAME)) {
      continue;
    }

    pid_t pid;

    if (RUNIT_RUNNING != pid_runit_service(&process->cfg, &pid)) {
      piksi_log(
        LOG_ERR,
        "Asked to tell skylark_daemon to reconnect (in download mode), but it isn't running");
      return false;
    }

    int ret = kill(pid, SIGUSR1);

    if (ret != 0) {
      piksi_log(LOG_ERR,
                "skylark_reconnect_dl: kill (SIGUSR1) pid %d error (%d) \"%s\"",
                pid,
                errno,
                strerror(errno));

      return false;
    }
  }

  return true;
}

static const char *const skylark_mode_enum_names[] = {"Disabled",
                                                      "HTTP 1.1",
                                                      "HTTP 2",
                                                      NULL};

enum {
  SKYLARK_MODE_DISABLED,
  SKYLARK_MODE_HTTP_1_1,
  SKYLARK_MODE_HTTP_2,
};

void skylark_settings_init(pk_settings_ctx_t *settings_ctx)
{
  mkfifo(UPLOAD_FIFO_FILE_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  mkfifo(DOWNLOAD_FIFO_FILE_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  settings_type_t skylark_mode;
  pk_settings_type_register_enum(settings_ctx,
                                 skylark_mode_enum_names,
                                 &skylark_mode);

  pk_settings_register(settings_ctx,
                       "skylark",
                       "mode",
                       &skylark_enabled,
                       sizeof(skylark_enabled),
                       skylark_mode,
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
