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
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/logging.h>
#include <libsbp/logging.h>

#include "cellmodem.h"
#include "cellmodem_inotify.h"

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

typedef struct {
  sbp_zmq_pubsub_ctx_t *pubsub_ctx;
  zloop_t *loop;
  zmq_pollitem_t pollitem;
  int inotify_fd;
  int watch_descriptor;
  char cellmodem_dev[32];
} inotify_ctx_t;

bool cellmodem_tty_exists(const char* path) {

  char full_path[PATH_MAX];
  snprintf(full_path, sizeof(full_path), "/dev/%s", path);

  struct stat buf;
  int ret = stat(full_path, &buf);

  return ret == 0 && S_ISCHR(buf.st_mode);
}

static void inotify_cleanup(inotify_ctx_t *ctx)
{
  zloop_poller_end(ctx->loop, &ctx->pollitem);

  inotify_rm_watch(ctx->inotify_fd, ctx->watch_descriptor);  
  close(ctx->inotify_fd);

  free(ctx);
}

static int inotify_output_cb(zloop_t *loop, zmq_pollitem_t *item, void *arg) 
{
  inotify_ctx_t *ctx = (inotify_ctx_t*) arg;

  char buf[BUF_LEN] __attribute__ ((aligned(8)));
  ssize_t count = read(ctx->inotify_fd, buf, BUF_LEN);

  if (count == 0) {
    piksi_log(LOG_ERR, "inotify read failed");
    return 0;
  }

  if (count < -1) {
    piksi_log(LOG_ERR, "inotify other error");
    return 0;
  }

  for (char *p = buf; p < buf + count; ) {

    struct inotify_event *event = (struct inotify_event *) p;

    if (event->mask & IN_CREATE) {
      if (strcmp(event->name, ctx->cellmodem_dev) == 0) {
        if (cellmodem_tty_exists(ctx->cellmodem_dev)) {

          piksi_log(LOG_DEBUG, "cell modem device created, starting pppd...");
          handle_pppd_respawn(ctx->pubsub_ctx);
          inotify_cleanup(ctx);

        } else {
          piksi_log(LOG_WARNING, "inotify said the cell modem should exist, but it doesn't...");
        }
      } else {
        piksi_log(LOG_DEBUG, "got notification that '%s' was created", event->name);
      }
    } else {
      piksi_log(LOG_WARNING, "unhandled inotify event");
    }
    
    p += sizeof(struct inotify_event) + event->len;
  }

  return 0;
}

void async_wait_for_tty(sbp_zmq_pubsub_ctx_t *pubsub_ctx, const char *cellmodem_dev)
{
  int inotify_fd = inotify_init1(IN_NONBLOCK);

  if (inotify_fd < 0) {
    piksi_log(LOG_DEBUG, "inotify init failed: %d", errno);
    goto fail;
  }

  int watch_descriptor = inotify_add_watch(inotify_fd, "/dev", IN_CREATE);

  if (watch_descriptor < 0) {
    piksi_log(LOG_DEBUG, "inotify add failed: %d", errno);
    goto fail;
  }

  inotify_ctx_t *ctx = calloc(1, sizeof(*ctx));

  ctx->pubsub_ctx = pubsub_ctx;
  ctx->inotify_fd = inotify_fd;

  strncpy(ctx->cellmodem_dev, cellmodem_dev, sizeof(ctx->cellmodem_dev));
  piksi_log(LOG_DEBUG, "registering to be notified when '%s' is created", ctx->cellmodem_dev);

  ctx->pollitem.fd = inotify_fd;
  ctx->pollitem.events = ZMQ_POLLIN|ZMQ_POLLERR;

  ctx->loop = sbp_zmq_pubsub_zloop_get(pubsub_ctx);
  zloop_poller(ctx->loop, &ctx->pollitem, inotify_output_cb, ctx);

  // Check if the device was created while were registering
  if (cellmodem_tty_exists(cellmodem_dev)) {

    piksi_log(LOG_DEBUG, "modem device was created while we were registering");
    inotify_cleanup(ctx);

    handle_pppd_respawn(pubsub_ctx);
  }

  return;

fail:
  piksi_log(LOG_DEBUG, "modem detection error, waiting to retry");
  zloop_timer(sbp_zmq_pubsub_zloop_get(pubsub_ctx), 5000, 1, pppd_respawn, pubsub_ctx);
}
