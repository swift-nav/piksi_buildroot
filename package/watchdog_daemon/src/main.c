/*
 * Copyright (C) 2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/logging.h>
#include <libpiksi/loop.h>
#include <libpiksi/util.h>
#include <libpiksi/settings_client.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>
#include <limits.h>

#include <semaphore.h>

#define PROGRAM_NAME "watchdog_daemon"
#define SETTINGS_METRICS_NAME ("settings/" PROGRAM_NAME)

#define WATCHDOG_POLL_PERIOD 1000

const char const *watchdog_sem_names[] = {
  "rpmsg_piksi",
  "sbp_router",
  "ports_daemon",
};
#define WATCHDOG_SEM_COUNT (sizeof(watchdog_sem_names) / sizeof(watchdog_sem_names[0]))
static sem_t *watchdog_sem[WATCHDOG_SEM_COUNT];

static int hw_watchdog_fd;
static bool hw_watchdog_enabled = true;
static uint32_t watchdog_sem_flags = 0;

static void watchdog_sem_poll(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)handle;
  (void)status;
  (void)context;

  /* Poll watchdog semaphores */
  for (unsigned i = 0; i < WATCHDOG_SEM_COUNT; i++) {
    if ((watchdog_sem[i] == SEM_FAILED) || (sem_trywait(watchdog_sem[i])) == 0) {
      watchdog_sem_flags &= ~(1 << i);
    }
  }

  /* Kick hardware watchdog if all serviced */
  if (watchdog_sem_flags == 0) {
    watchdog_sem_flags = (1 << WATCHDOG_SEM_COUNT) - 1;
    ioctl(hw_watchdog_fd, WDIOC_KEEPALIVE, 0);
  }
}

static void watchdog_sem_setup(pk_loop_t *loop)
{
  for (unsigned i = 0; i < WATCHDOG_SEM_COUNT; i++) {
    char name[NAME_MAX];
    snprintf(name, sizeof(name), "/wdg-%s", watchdog_sem_names[i]);
    watchdog_sem[i] = sem_open(name, O_CREAT, 0666, 0);
    if (watchdog_sem[i] == SEM_FAILED) {
      piksi_log(LOG_SBP | LOG_WARNING,
                "Failed to open watchdog semaphore %s: %s",
                watchdog_sem_names[i],
                strerror(errno));
    }
  }
  pk_loop_timer_add(loop, WATCHDOG_POLL_PERIOD, watchdog_sem_poll, NULL);
}

static int watchdog_notify_enable(void *context)
{
  (void)context;
  int options;
  if (hw_watchdog_enabled) {
    options = WDIOS_ENABLECARD;
  } else {
    options = WDIOS_DISABLECARD;
  }
  ioctl(hw_watchdog_fd, WDIOC_SETOPTIONS, &options);
  return SETTINGS_WR_OK;
}

int main(void)
{
  int ret = EXIT_FAILURE;
  logging_init(PROGRAM_NAME);
  pk_loop_t *loop = NULL;
  pk_settings_ctx_t *settings_ctx = NULL;

  hw_watchdog_fd = open("/dev/watchdog0", O_RDWR);
  if (hw_watchdog_fd < 0) {
    fprintf(stderr, "Failed to open hardware watchdog device\n");
    exit(EXIT_FAILURE);
  }

  loop = pk_loop_create();
  if (loop == NULL) {
    goto cleanup;
  }

  watchdog_sem_setup(loop);

  /* Set up settings */
  settings_ctx = pk_settings_create(SETTINGS_METRICS_NAME);
  if (settings_ctx == NULL) {
    fprintf(stderr, "Failed to create settings context\n");
    goto cleanup;
  }

  if (pk_settings_attach(settings_ctx, loop) != 0) {
    fprintf(stderr, "Failed to create settings context\n");
    goto cleanup;
  }

  pk_settings_register(settings_ctx,
                       "experimental_flags",
                       "hw_watchdog_enabled",
                       &hw_watchdog_enabled,
                       sizeof(hw_watchdog_enabled),
                       SETTINGS_TYPE_BOOL,
                       watchdog_notify_enable,
                       NULL);

  pk_loop_run_simple(loop);

  ret = EXIT_SUCCESS;

cleanup:
  pk_loop_destroy(&loop);
  logging_deinit();
  exit(ret);
}
