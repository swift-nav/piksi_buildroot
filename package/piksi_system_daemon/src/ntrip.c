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
static char ntrip_url[256];

static int ntrip_notify(void *context)
{
  (void)context;

  FILE *f = fopen("/var/run/ntrip_daemon_cmd", "wb");
  if (f == NULL) {
      piksi_log(LOG_ERR, "Error writing ntrip_daemon_cmd");
      return -1;
  }
  fprintf(f, "--url %s", ntrip_url);
  fclose(f);

  char exec[128];
  if (ntrip_enabled) {
    fprintf(stdout, "Starting group ntrip\n");
    snprintf(exec, sizeof(exec), "/usr/bin/monit -g ntrip restart");
  } else {
    fprintf(stdout, "Stopping group ntrip\n");
    snprintf(exec, sizeof(exec), "/usr/bin/monit -g ntrip stop");
  }

  FILE *p = popen(exec, "r");
  if (p == NULL) {
    piksi_log(LOG_ERR, "Unable to monit group ntrip");
    return -1;
  } 

  int status = pclose(p);
  if (status == -1) {
    piksi_log(LOG_ERR, "Error %d while closing exec pipe", status);
    return -1;
  }

  return 0;
}

void ntrip_init(settings_ctx_t *settings_ctx)
{
  if (access(FIFO_FILE_PATH, F_OK) != 0) {
    mkfifo(FIFO_FILE_PATH, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  }

  settings_register(settings_ctx, "ntrip", "enable",
                    &ntrip_enabled, sizeof(ntrip_enabled),
                    SETTINGS_TYPE_BOOL,
                    ntrip_notify, NULL);

  settings_register(settings_ctx, "ntrip", "url",
                    &ntrip_url, sizeof(ntrip_url),
                    SETTINGS_TYPE_STRING,
                    ntrip_notify, NULL);
}
