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
#include <libpiksi/logging.h>
#include <libsbp/navigation.h>
#include <libsbp/sbp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rtcm3_sbp.h>
#include "sbp.h"

#define PROGRAM_NAME "sbp_nmea_bridge"

#define NMEA_SUB_ENDPOINT  "ipc:///var/run/sockets/nmea_internal.pub"  /* NMEA Internal Out */

bool nmea_debug = false;

struct sbp_nmea_state state;

static int gpgga_rate;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMisc options");
  puts("\t--debug");
}

static void nmea_callback(uint8_t nmea_str[]){
  /* Need to output nmea string here somehow */
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

static int notify_gga_rate_changed(void *context)
{
  (void)context;
  sbp2nmea_gpgga_rate(gpgga_rate,state);
  return 0;
}

static void gps_time_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  (void) sender_id;
  (void) len;
  msg_gps_time_t *gps_time = (msg_gps_time_t*)msg;
  sbp2nmea_gps_time(gps_time,&state);
}

static void gps_time_reference_callback(u16 sender_id, u8 len, u8 msg[], void* context) {
  (void) len;
  (void) msg;
  (void) context;
  sbp2nmea_set_base_id(sender_id,&state);
}

static void utc_time_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  (void) sender_id;
  (void) len;
  msg_utc_time_t *utc_time = (msg_utc_time_t*)msg;
  sbp2nmea_utc_time(utc_time,&state);
}

static void age_corrections_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  (void) sender_id;
  (void) len;
  msg_age_corrections_t *age_corr = (msg_age_corrections_t*)msg;
  sbp2nmea_age_corrections(age_corr,&state);
}

static void dops_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  (void) sender_id;
  (void) len;
  msg_dops_t *dops = (msg_age_corrections_t*)msg;
  sbp2nmea_dops(dops,&state);
}

static void pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  (void) sender_id;
  (void) len;
  msg_pos_llh_t *pos_llh = (msg_pos_llh_t*)msg;
  sbp2nmea_pos_llh(pos_llh,&state);
}

static void vel_ned_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  (void) sender_id;
  (void) len;
  msg_vel_ned_t *vel_ned = (msg_vel_ned_t*)msg;
  sbp2nmea_vel_ned(pos_llh,&state);
}

static int cleanup(pk_endpoint_t **rtcm_ept_loc, int status);

int main(int argc, char *argv[])
{
  settings_ctx_t *settings_ctx = NULL;
  pk_loop_t *loop = NULL;
  pk_endpoint_t *nmea_sub = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(cleanup(&nmea_sub, EXIT_FAILURE));
  }

  /* Need to init state variable before we get SBP in */
  sbp2nmea_init(&state,nmea_callback);

  if (sbp_init() != 0) {
    piksi_log(LOG_ERR, "error initializing SBP");
    exit(cleanup(&nmea_sub, EXIT_FAILURE));
  }

  loop = sbp_get_loop();
  if (loop == NULL) {
    exit(cleanup(&nmea_sub, EXIT_FAILURE));
  }

  nmea_sub = pk_endpoint_create(NMEA_SUB_ENDPOINT, PK_ENDPOINT_SUB);
  if (nmea_sub == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(cleanup(&nmea_sub, EXIT_FAILURE));
  }

  if (pk_loop_endpoint_reader_add(loop, nmea_sub, nmea_reader_handler, nmea_sub)
      == NULL) {
    piksi_log(LOG_ERR, "error adding reader");
    exit(cleanup(&nmea_sub, EXIT_FAILURE));
  }

  if (sbp_callback_register(SBP_MSG_GPS_TIME, gps_time_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting GPS TIME callback");
    exit(cleanup(&nmea_sub, EXIT_FAILURE));
  }

  if (sbp_callback_register(SBP_MSG_UTC_TIME, utc_time_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting UTC TIME callback");
    return cleanup(&nmea_sub, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_DOPS, dops_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting dops callback");
    return cleanup(&nmea_sub, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_AGE_CORRECTIONS, age_corrections_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting Age of Corrections callback");
    return cleanup(&nmea_sub, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_POS_LLH, pos_llh_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting pos llh callback");
    return cleanup(&nmea_sub, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_VEL_NED, vel_ned_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting vel NED callback");
    return cleanup(&nmea_sub, EXIT_FAILURE);
  }

  settings_ctx = sbp_get_settings_ctx();

  settings_add_watch(settings_ctx, "nmea", "gpgga_rate",
                                        &gpgga_rate , sizeof(gpgga_rate),
                                        SETTINGS_TYPE_INT,
                                        notify_gpgga_rate_changed, NULL);


  sbp_run();

  exit(cleanup(&nmea_sub, EXIT_SUCCESS));
}

static int cleanup(pk_endpoint_t **nmea_ept_loc, int status)
{
  pk_endpoint_destroy(nmea_ept_loc);
  sbp_deinit();
  logging_deinit();
  return status;
}
