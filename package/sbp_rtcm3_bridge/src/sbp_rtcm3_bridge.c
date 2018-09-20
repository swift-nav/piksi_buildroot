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

#include <assert.h>
#include <getopt.h>
#include <gnss-converters/rtcm3_sbp.h>
#include <libpiksi/logging.h>
#include <libsbp/navigation.h>
#include <libsbp/sbp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sbp.h"

#define PROGRAM_NAME "sbp_rtcm3_bridge"

#define RTCM3_SUB_ENDPOINT "ipc:///var/run/sockets/rtcm3_internal.pub" /* RTCM3 Internal Out */

bool rtcm3_debug = false;

struct rtcm3_sbp_state state;

bool simulator_enabled_watch = false;

static int rtcm2sbp_decode_frame_shim(const u8 *data, const size_t length, void *context)
{
  rtcm2sbp_decode_frame(data, length, context);
  return 0;
}

static void rtcm3_reader_handler(pk_loop_t *loop, void *handle, void *context)
{
  (void)loop;
  (void)handle;
  pk_endpoint_t *rtcm_sub_ept = (pk_endpoint_t *)context;
  if (pk_endpoint_receive(rtcm_sub_ept, rtcm2sbp_decode_frame_shim, &state) != 0) {
    piksi_log(LOG_ERR,
              "%s: error in %s (%s:%d): %s",
              __FUNCTION__,
              "pk_endpoint_receive",
              __FILE__,
              __LINE__,
              pk_endpoint_strerror());
  }
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
    {"debug", no_argument, 0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_DEBUG: {
      rtcm3_debug = true;
    } break;

    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }
  return 0;
}

static void gps_time_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  (void)sender_id;
  (void)len;
  msg_gps_time_t *time = (msg_gps_time_t *)msg;

  if ((time->flags & TIME_SOURCE_MASK) == NO_TIME) {
    return;
  }

  gps_time_sec_t gps_time;
  gps_time.tow = time->tow * 0.001;
  gps_time.wn = time->wn;
  rtcm2sbp_set_gps_time(&gps_time, &state);
}

static void utc_time_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  (void)sender_id;
  (void)len;
  msg_utc_time_t *time = (msg_utc_time_t *)msg;

  if ((time->flags & TIME_SOURCE_MASK) == NO_TIME) {
    return;
  }

  /* work out the time of day in utc time */
  u32 utc_tod = time->hours * 3600 + time->minutes * 60 + time->seconds;

  // Check we aren't within 0.1ms of the whole second boundary and round up if we are
  if (999999999 - time->ns < 1e6) {
    utc_tod += 1;
  }

  /* work out the gps time of day */
  u32 gps_tod = (time->tow % 86400000) * 1e-3;

  s8 leap_second;
  /* if gps tod is smaller than utc tod we've crossed the day boundary during the leap second */
  if (utc_tod < gps_tod) {
    leap_second = gps_tod - utc_tod;
  } else {
    leap_second = gps_tod + 86400 - utc_tod;
  }

  rtcm2sbp_set_leap_second(leap_second, &state);
}

static void ephemeris_glo_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  (void)sender_id;
  (void)len;
  msg_ephemeris_glo_t *e = (msg_ephemeris_glo_t *)msg;

  /* extract just the FCN field */
  rtcm2sbp_set_glo_fcn(e->common.sid, e->fcn, &state);
}

static int notify_simulator_enable_changed(void *context)
{
  (void)context;
  sbp_simulator_enabled_set(simulator_enabled_watch);
  return 0;
}

static int cleanup(pk_endpoint_t **rtcm_ept_loc, int status);

int main(int argc, char *argv[])
{
  settings_ctx_t *settings_ctx = NULL;
  pk_loop_t *loop = NULL;
  pk_endpoint_t *rtcm3_sub = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(cleanup(&rtcm3_sub, EXIT_FAILURE));
  }

  /* Need to init state variable before we get SBP in */
  rtcm2sbp_init(&state, sbp_message_send, sbp_base_obs_invalid, NULL);

  if (sbp_init() != 0) {
    piksi_log(LOG_ERR, "error initializing SBP");
    exit(cleanup(&rtcm3_sub, EXIT_FAILURE));
  }

  loop = sbp_get_loop();
  if (loop == NULL) {
    exit(cleanup(&rtcm3_sub, EXIT_FAILURE));
  }

  rtcm3_sub = pk_endpoint_create(RTCM3_SUB_ENDPOINT, PK_ENDPOINT_SUB);
  if (rtcm3_sub == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(cleanup(&rtcm3_sub, EXIT_FAILURE));
  }

  if (pk_loop_endpoint_reader_add(loop, rtcm3_sub, rtcm3_reader_handler, rtcm3_sub) == NULL) {
    piksi_log(LOG_ERR, "error adding reader");
    exit(cleanup(&rtcm3_sub, EXIT_FAILURE));
  }

  if (sbp_callback_register(SBP_MSG_GPS_TIME, gps_time_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting GPS TIME callback");
    exit(cleanup(&rtcm3_sub, EXIT_FAILURE));
  }

  if (sbp_callback_register(SBP_MSG_UTC_TIME, utc_time_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting UTC TIME callback");
    return cleanup(&rtcm3_sub, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_EPHEMERIS_GLO_DEP_D, ephemeris_glo_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting EPHEMERIS GLO callback");
    return cleanup(&rtcm3_sub, EXIT_FAILURE);
  }
  if (sbp_callback_register(SBP_MSG_EPHEMERIS_GLO, ephemeris_glo_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting EPHEMERIS GLO callback");
    return cleanup(&rtcm3_sub, EXIT_FAILURE);
  }

  settings_ctx = sbp_get_settings_ctx();

  settings_add_watch(settings_ctx,
                     "simulator",
                     "enabled",
                     &simulator_enabled_watch,
                     sizeof(simulator_enabled_watch),
                     SETTINGS_TYPE_BOOL,
                     notify_simulator_enable_changed,
                     NULL);

  sbp_run();

  exit(cleanup(&rtcm3_sub, EXIT_SUCCESS));
}

static int cleanup(pk_endpoint_t **rtcm_ept_loc, int status)
{
  pk_endpoint_destroy(rtcm_ept_loc);
  sbp_deinit();
  logging_deinit();
  return status;
}
