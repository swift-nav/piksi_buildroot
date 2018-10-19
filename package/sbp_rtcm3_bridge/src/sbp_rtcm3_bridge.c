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

#include "sbp.h"
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

#define PROGRAM_NAME "sbp_rtcm3_bridge"

#define RTCM3_SUB_ENDPOINT "ipc:///var/run/sockets/rtcm3_internal.pub" /* RTCM3 Internal Out */
#define RTCM3_PUB_ENDPOINT "ipc:///var/run/sockets/rtcm3_internal.sub" /* RTCM3 Internal In */

bool rtcm3_debug = false;

struct rtcm3_sbp_state rtcm3_to_sbp_state;
struct rtcm3_out_state sbp_to_rtcm3_state;

bool simulator_enabled_watch = false;

pk_endpoint_t *rtcm3_pub = NULL;

static const char *const rtcm_out_modes[] = {"Legacy", "MSM4", "MSM5", NULL};
enum { RTCM_OUT_MODE_LEGACY, RTCM_OUT_MODE_MSM4, RTCM_OUT_MODE_MSM5 };
static u8 rtcm_out_mode = (u8)RTCM_OUT_MODE_MSM5;

static int rtcm2sbp_decode_frame_shim(const u8 *data, const size_t length, void *context)
{
  rtcm2sbp_decode_frame(data, length, context);
  return 0;
}

static void rtcm3_out_callback(u8 *buffer, u16 length, void *context)
{
  (void)context;
  if (pk_endpoint_send(rtcm3_pub, buffer, length) != 0) {
    piksi_log(LOG_ERR, "Error sending rtcm3 message to rtcm3_internal");
  }
}

static void rtcm3_reader_handler(pk_loop_t *loop, void *handle, void *context)
{
  (void)loop;
  (void)handle;
  pk_endpoint_t *rtcm_sub_ept = (pk_endpoint_t *)context;
  if (pk_endpoint_receive(rtcm_sub_ept, rtcm2sbp_decode_frame_shim, &rtcm3_to_sbp_state) != 0) {
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

  gps_time_t gps_time;
  gps_time.tow = time->tow * 0.001;
  gps_time.wn = time->wn;
  rtcm2sbp_set_gps_time(&gps_time, &rtcm3_to_sbp_state);
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

  /* Check we aren't within 0.1ms of the whole second boundary and round up if we are */
  if (999999999 - time->ns < 1e6) {
    utc_tod += 1;
  }

  /* work out the gps time of day */
  u32 gps_tod = (time->tow % 86400000) * 1e-3;

  s8 leap_second;
  /* if gps tod is smaller than utc tod we've crossed the day boundary during
   * the leap second */
  if (utc_tod < gps_tod) {
    leap_second = gps_tod - utc_tod;
  } else {
    leap_second = gps_tod + 86400 - utc_tod;
  }

  rtcm2sbp_set_leap_second(leap_second, &rtcm3_to_sbp_state);
  sbp2rtcm_set_leap_second(leap_second, &sbp_to_rtcm3_state);
}

static void ephemeris_glo_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  (void)sender_id;
  (void)len;
  msg_ephemeris_glo_t *e = (msg_ephemeris_glo_t *)msg;

  /* extract just the FCN field */
  rtcm2sbp_set_glo_fcn(e->common.sid, e->fcn, &rtcm3_to_sbp_state);
  sbp2rtcm_set_glo_fcn(e->common.sid, e->fcn, &sbp_to_rtcm3_state);
}

static void base_pos_ecef_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  sbp2rtcm_base_pos_ecef_cb(sender_id, len, msg, &sbp_to_rtcm3_state);
}

static void glo_bias_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  sbp2rtcm_glo_biases_cb(sender_id, len, msg, &sbp_to_rtcm3_state);
}

static void obs_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  sbp2rtcm_sbp_obs_cb(sender_id, len, msg, &sbp_to_rtcm3_state);
}

static int notify_simulator_enable_changed(void *context)
{
  (void)context;
  sbp_simulator_enabled_set(simulator_enabled_watch);
  return 0;
}

static int notify_rtcm_out_output_mode_changed(void *context)
{
  (void)context;

  switch (rtcm_out_mode) {
  case RTCM_OUT_MODE_LEGACY: sbp2rtcm_set_rtcm_out_mode(MSM_UNKNOWN, &sbp_to_rtcm3_state); break;
  case RTCM_OUT_MODE_MSM4: sbp2rtcm_set_rtcm_out_mode(MSM4, &sbp_to_rtcm3_state); break;
  case RTCM_OUT_MODE_MSM5: sbp2rtcm_set_rtcm_out_mode(MSM5, &sbp_to_rtcm3_state); break;
  default: return -1;
  }

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

  /* Need to init rtcm3_to_sbp_state and sbp_to_rtcm3_state variables before
     we get SBP in */
  rtcm2sbp_init(&rtcm3_to_sbp_state, sbp_message_send, sbp_base_obs_invalid, NULL);
  sbp2rtcm_init(&sbp_to_rtcm3_state, rtcm3_out_callback, NULL);

  if (sbp_init() != 0) {
    piksi_log(LOG_ERR, "error initializing SBP");
    exit(cleanup(&rtcm3_sub, EXIT_FAILURE));
  }

  rtcm3_pub = pk_endpoint_create(RTCM3_PUB_ENDPOINT, PK_ENDPOINT_PUB);
  if (rtcm3_pub == NULL) {
    piksi_log(LOG_ERR, "error creating PUB socket");
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

  if (sbp_callback_register(SBP_MSG_BASE_POS_ECEF, base_pos_ecef_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting base pos ECEF callback");
    return cleanup(&rtcm3_sub, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_GLO_BIASES, glo_bias_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting GLO bias callback callback");
    return cleanup(&rtcm3_sub, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_OBS, obs_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting obs callback callback");
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

  settings_type_t settings_type_rtcm_out_mode;
  settings_type_register_enum(settings_ctx, rtcm_out_modes, &settings_type_rtcm_out_mode);
  settings_register(settings_ctx,
                    "rtcm_out",
                    "output_mode",
                    &rtcm_out_mode,
                    sizeof(rtcm_out_mode),
                    settings_type_rtcm_out_mode,
                    notify_rtcm_out_output_mode_changed,
                    &rtcm_out_mode);

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
