/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/logging.h>
#include <libsbp/logging.h>

#include "cell_modem_settings.h"
#include "cell_modem_inotify.h"
#include "cell_modem_probe.h"

#define PPPD_RESPAWN_TIMEOUT (5000u)
#define OVERRIDE_RETRY_TIMER_PERIOD (1000u)
#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

typedef struct inotify_ctx_s {
  pk_loop_t *loop;
  void *pollitem;
  int inotify_fd;
  int watch_descriptor;
  char *cell_modem_dev;
  enum modem_type modem_type;
} inotify_ctx_t;

static void inotify_output_cb(pk_loop_t *loop, void *poll_handle, void *context);

inotify_ctx_t * inotify_ctx_create(const char *path,
                                   int inotify_init_flags,
                                   uint32_t inotify_watch_flags,
                                   pk_loop_t *loop)
{
  inotify_ctx_t *ctx = calloc(1, sizeof(inotify_ctx_t));
  if (ctx == NULL) {
    goto failure;
  }

  ctx->inotify_fd = inotify_init1(inotify_init_flags);
  if (ctx->inotify_fd == -1) {
    piksi_log(LOG_DEBUG, "inotify init failed: %d", errno);
    goto failure;
  }

  ctx->watch_descriptor = inotify_add_watch(ctx->inotify_fd, path, inotify_watch_flags);
  if (ctx->watch_descriptor == -1) {
    piksi_log(LOG_DEBUG, "inotify add failed: %d", errno);
    goto failure;
  }

  ctx->loop = loop;
  ctx->pollitem = pk_loop_poll_add(ctx->loop, ctx->inotify_fd, inotify_output_cb, ctx);
  if (ctx->pollitem == NULL) {
    piksi_log(LOG_DEBUG, "inotify poll add failed");
    goto failure;
  }

  return ctx;

failure:
  inotify_ctx_destroy(&ctx);
  return NULL;
}

void inotify_ctx_destroy(inotify_ctx_t **ctx_loc)
{
  if (ctx_loc == NULL || *ctx_loc == NULL) {
   return;
  }
  inotify_ctx_t *ctx = *ctx_loc;
  if (ctx->inotify_fd >= 0) {
    if (ctx->watch_descriptor != -1) {
      inotify_rm_watch(ctx->inotify_fd, ctx->watch_descriptor);
    }
    close(ctx->inotify_fd);
  }
  if (ctx->pollitem) {
    pk_loop_remove_handle(ctx->pollitem);
  }
  free(ctx);
  *ctx_loc = NULL;
}

bool cell_modem_tty_exists(const char* path) {

  char full_path[PATH_MAX];
  snprintf(full_path, sizeof(full_path), "/dev/%s", path);

  struct stat buf;
  int ret = stat(full_path, &buf);

  return ret == 0 && S_ISCHR(buf.st_mode);
}

static int update_dev_from_probe(inotify_ctx_t *ctx, char *dev)
{
  if (ctx == NULL) {
    return 1;
  }
  if (dev == NULL || dev[0] == '\0') {
    return 1;
  }
  if (!cell_modem_tty_exists(dev)) {
    piksi_log(LOG_WARNING,
              "Update dev tty does not exist: '%s'",
              dev);
    return 1;
  }
  char *dev_override = cell_modem_get_dev_override();
  if (dev_override != NULL && strcmp(dev_override, dev) != 0) {
    return 1;
  }
  ctx->modem_type = cell_modem_probe(dev);
  if (ctx->modem_type != MODEM_TYPE_INVALID) {
    ctx->cell_modem_dev = strdup(dev);
    cell_modem_set_dev(ctx->cell_modem_dev, ctx->modem_type);
    return 0;
  } else if (dev_override != NULL && strcmp(dev_override, dev) == 0) {
    piksi_log(LOG_WARNING,
              "Override device failed probe: %s",
              dev);
    return -1;
  }
  return 1;
}

void cell_modem_set_dev_to_invalid(inotify_ctx_t *ctx)
{
  if (ctx == NULL) {
    return;
  }
  ctx->modem_type = MODEM_TYPE_INVALID;
  if (ctx->cell_modem_dev != NULL) {
    free(ctx->cell_modem_dev);
    ctx->cell_modem_dev = NULL;
  }
  cell_modem_set_dev(NULL, MODEM_TYPE_INVALID);
}

int cell_modem_scan_for_modem(inotify_ctx_t *ctx)
{
  int result = -1;
  if (ctx == NULL) {
    return -1;
  }
  /* Try what's already here.  Inotify will only tell us about new devs */
  DIR *dir = opendir("/dev");
  for (struct dirent *ent = readdir(dir); ent; ent = readdir(dir)) {
    if (ent->d_type == DT_CHR) {
      result = update_dev_from_probe(ctx, ent->d_name);
      if (result <= 0) {
        break;
      }
    }
  }
  closedir(dir);

  return result;
}

static void inotify_output_cb(pk_loop_t *loop, void *poll_handle, void *context)
{
  (void) loop;
  (void) poll_handle;

  inotify_ctx_t *ctx = (inotify_ctx_t*) context;

  char buf[BUF_LEN] __attribute__ ((aligned(__alignof__(struct inotify_event))));
  ssize_t count = read(ctx->inotify_fd, buf, BUF_LEN);

  if (count == 0) {
    piksi_log(LOG_ERR, "inotify read failed");
  }

  if (count < -1) {
    piksi_log(LOG_ERR, "inotify other error");
  }

  for (char* p = buf; p < buf + count; ) {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    struct inotify_event *event = (struct inotify_event *) p;
#pragma GCC diagnostic pop

    if (event->mask & IN_CREATE) {
      piksi_log(LOG_DEBUG, "got notification that '%s' was created", event->name);
      if ((ctx->modem_type == MODEM_TYPE_INVALID) && cell_modem_tty_exists(event->name)) {
        usleep(100000);
        if (update_dev_from_probe(ctx, event->name) == 0) {
          piksi_log(LOG_DEBUG, "set modem device to '%s' from notification", event->name);
        }
      }
    } else if (event->mask & IN_DELETE) {
      piksi_log(LOG_DEBUG, "got notification that '%s' was deleted", event->name);
      if ((ctx->cell_modem_dev != NULL) &&
          (strcmp(ctx->cell_modem_dev, event->name) == 0)) {
        cell_modem_set_dev_to_invalid(ctx);
      }
    } else {
      piksi_log(LOG_WARNING, "unhandled inotify event");
    }

    p += sizeof(struct inotify_event) + event->len;
  }
}

inotify_ctx_t * async_wait_for_tty(pk_loop_t *loop)
{
  inotify_ctx_t *ctx = inotify_ctx_create("/dev",
                                          IN_NONBLOCK,
                                          IN_CREATE | IN_DELETE,
                                          loop);
  if (ctx == NULL) {
    piksi_log(LOG_DEBUG, "inotify ctx create failed");
    goto fail;
  }

  if (cell_modem_scan_for_modem(ctx) != 0) {
    piksi_log(LOG_DEBUG, "inital modem scan failed, waiting to retry");
    pk_loop_timer_add(loop,
                      OVERRIDE_RETRY_TIMER_PERIOD,
                      override_probe_retry,
                      ctx);
  }

  return ctx;

fail:
  piksi_log(LOG_DEBUG, "failed to create modem detection context, waiting to retry");
  pk_loop_timer_add(loop, PPPD_RESPAWN_TIMEOUT, pppd_respawn, NULL);
  return NULL;
}
