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

#include <assert.h>
#include <czmq.h>
#include <getopt.h>
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>
#include <libpiksi/logging.h>
#include <libsbp/navigation.h>
#include <libsbp/sbp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rtcm3_sbp.h>
#include "sbp.h"

#define PROGRAM_NAME "sbp_rtcm3_bridge"

#define RTCM3_SUB_ENDPOINT  ">tcp://127.0.0.1:45010"  /* RTCM3 Internal Out */
#define SBP_SUB_ENDPOINT    ">tcp://127.0.0.1:43030"  /* SBP External Out */
#define SBP_PUB_ENDPOINT    ">tcp://127.0.0.1:43031"  /* SBP External In */

bool rtcm3_debug = false;

struct rtcm3_sbp_state state;

bool simulator_enabled_watch = false;

static int rtcm3_reader_handler(zloop_t *zloop, zsock_t *zsock, void *arg)
{
  (void)zloop;
  (void)arg;
  zmsg_t *msg;
  while (1) {
    msg = zmsg_recv(zsock);
    if (msg != NULL) {
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in zmsg_recv()");
      return -1;
    }
  }

  zframe_t *frame;
  for (frame = zmsg_first(msg); frame != NULL; frame = zmsg_next(msg)) {
    rtcm2sbp_decode_frame(zframe_data(frame), zframe_size(frame), &state);
  }

  zmsg_destroy(&msg);
  return 0;
}

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMisc options");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_DEBUG = 1,
  };

  const struct option long_opts[] = {
    {"debug", no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case OPT_ID_DEBUG: {
        rtcm3_debug = true;
      }
        break;

      default: {
        puts("Invalid option");
        return -1;
      }
        break;
    }
  }
  return 0;
}

static void gps_time_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  (void) sender_id;
  (void) len;
  msg_gps_time_t *time = (msg_gps_time_t*)msg;

  if((time->flags & TIME_SOURCE_MASK) == NO_TIME) {
    return;
  }

  gps_time_sec_t gps_time;
  gps_time.tow = time->tow * 0.001;
  gps_time.wn = time->wn;
  rtcm2sbp_set_gps_time(&gps_time,&state);
}

static void utc_time_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  (void) sender_id;
  (void) len;
  msg_utc_time_t *time = (msg_utc_time_t*)msg;

  if((time->flags & TIME_SOURCE_MASK) == NO_TIME) {
    return;
  }

  /* work out the time of day in utc time */
  u32 utc_tod = time->hours * 3600 + time->minutes * 60 + time->seconds;

  // Check we aren't within 0.1ms of the whole second boundary and round up if we are
  if (999999999 - time->ns < 1e6) {
    utc_tod += 1;
  }

  /* work out the gps time of day */
  u32 gps_tod = (time->tow  % 86400000) * 1e-3;

  s8 leap_second;
  /* if gps tod is smaller than utc tod we've crossed the day boundary during the leap second */
  if (utc_tod < gps_tod) {
    leap_second = gps_tod - utc_tod;
  } else {
    leap_second = gps_tod + 86400 - utc_tod;
  }

  rtcm2sbp_set_leap_second(leap_second,&state);
}

static int notify_simulator_enable_changed(void *context)
{
  (void)context;
  sbp_simulator_enabled_set(simulator_enabled_watch);
  return 0;
}

static int cleanup(settings_ctx_t **settings_ctx_loc,
                   sbp_zmq_pubsub_ctx_t **pubsub_ctx_loc,
                   int status);

int main(int argc, char *argv[])
{
  settings_ctx_t *settings_ctx = NULL;
  sbp_zmq_pubsub_ctx_t *ctx = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  /* Need to init state variable before we get SBP in */
  rtcm2sbp_init(&state, sbp_message_send, sbp_base_obs_invalid);

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  ctx = sbp_zmq_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  zsock_t *rtcm3_sub = zsock_new_sub(RTCM3_SUB_ENDPOINT, "");
  if (rtcm3_sub == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  if (zloop_reader(sbp_zmq_pubsub_zloop_get(ctx), rtcm3_sub,
                   rtcm3_reader_handler, NULL) != 0) {
    piksi_log(LOG_ERR, "error adding reader");
    return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  if (sbp_init(sbp_zmq_pubsub_rx_ctx_get(ctx),
               sbp_zmq_pubsub_tx_ctx_get(ctx)) != 0) {
    piksi_log(LOG_ERR, "error initializing SBP");
    return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_GPS_TIME, gps_time_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting GPS TIME callback");
    return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_UTC_TIME, utc_time_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting UTC TIME callback");
    return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  settings_ctx = settings_create();

  if (settings_ctx == NULL) {
    sbp_log(LOG_ERR, "Error registering for settings!");
    return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  if (settings_reader_add(settings_ctx,
                          sbp_zmq_pubsub_zloop_get(ctx)) != 0) {
    sbp_log(LOG_ERR, "Error registering for settings read!");
    return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  settings_add_watch(settings_ctx, "simulator", "enabled",
                     &simulator_enabled_watch , sizeof(simulator_enabled_watch),
                     SETTINGS_TYPE_BOOL,
                     notify_simulator_enable_changed, NULL);

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

  return cleanup(&settings_ctx, &ctx, EXIT_SUCCESS);
}

static int cleanup(settings_ctx_t **settings_ctx_loc,
                   sbp_zmq_pubsub_ctx_t **pubsub_ctx_loc,
                   int status) {
  sbp_zmq_pubsub_destroy(pubsub_ctx_loc);
  settings_destroy(settings_ctx_loc);
  logging_deinit();

  return status;
}
