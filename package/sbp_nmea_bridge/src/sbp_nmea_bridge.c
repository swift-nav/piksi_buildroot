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
#include <gnss-converters/sbp_nmea.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libsbp/navigation.h>
#include <libsbp/orientation.h>
#include <libsbp/sbp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sbp.h"

#define PROGRAM_NAME "sbp_nmea_bridge"

#define NMEA_PUB_ENDPOINT "ipc:///var/run/sockets/nmea_internal.sub" /* NMEA Internal In */
#define NMEA_METRIC_NAME "nmea/pub"

bool nmea_debug = false;

pk_endpoint_t *nmea_pub = NULL;

static int gpgga_rate = 1;
static int gprmc_rate = 10;
static int gpvtg_rate = 1;
static int gphdt_rate = 1;
static int gpgll_rate = 10;
static int gpzda_rate = 10;
static int gsa_rate = 10;
static int gpgst_rate = 1;

static float soln_freq = 10.0;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMisc options");
  puts("\t--debug");
}

static void nmea_callback(uint8_t nmea_str[])
{
  size_t nmea_str_len = strlen((const char *)nmea_str);
  if (pk_endpoint_send(nmea_pub, nmea_str, nmea_str_len) != 0) {
    piksi_log(LOG_ERR, "Error sending nmea string to nmea_internal");
  }
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
      nmea_debug = true;
    } break;

    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }
  return 0;
}

static int notify_gpgga_rate_changed(void *context)
{
  sbp2nmea_rate_set(context, gpgga_rate, SBP2NMEA_NMEA_GGA);
  return SETTINGS_WR_OK;
}

static int notify_gprmc_rate_changed(void *context)
{
  sbp2nmea_rate_set(context, gprmc_rate, SBP2NMEA_NMEA_RMC);
  return SETTINGS_WR_OK;
}

static int notify_gpvtg_rate_changed(void *context)
{
  sbp2nmea_rate_set(context, gpvtg_rate, SBP2NMEA_NMEA_VTG);
  return SETTINGS_WR_OK;
}

static int notify_gphdt_rate_changed(void *context)
{
  sbp2nmea_rate_set(context, gphdt_rate, SBP2NMEA_NMEA_HDT);
  return SETTINGS_WR_OK;
}

static int notify_gpgll_rate_changed(void *context)
{
  sbp2nmea_rate_set(context, gpgll_rate, SBP2NMEA_NMEA_GLL);
  return SETTINGS_WR_OK;
}

static int notify_gpzda_rate_changed(void *context)
{
  sbp2nmea_rate_set(context, gpzda_rate, SBP2NMEA_NMEA_ZDA);
  return SETTINGS_WR_OK;
}

static int notify_gsa_rate_changed(void *context)
{
  sbp2nmea_rate_set(context, gsa_rate, SBP2NMEA_NMEA_GSA);
  return SETTINGS_WR_OK;
}

static int notify_gpgst_rate_changed(void *context)
{
  sbp2nmea_rate_set(context, gpgst_rate, SBP2NMEA_NMEA_GST);
  return SETTINGS_WR_OK;
}

static int notify_soln_freq_changed(void *context)
{
  sbp2nmea_soln_freq_set(context, soln_freq);
  return SETTINGS_WR_OK;
}

static void gps_time_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;
  (void)len;
  sbp2nmea(context, msg, SBP2NMEA_SBP_GPS_TIME);
}

static void baseline_heading_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;
  (void)len;
  sbp2nmea(context, msg, SBP2NMEA_SBP_HDG);
}

static void msg_obs_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)len;
  (void)msg;
  /* Must be outbound SBP sending */
  if (sbp_sender_id_get() == sender_id) {
    uint8_t num_obs = (len - sizeof(observation_header_t)) / sizeof(packed_obs_content_t);
    sbp2nmea_obs(context, (msg_obs_t *)msg, num_obs);
  } else if (sender_id > 0) {
    sbp2nmea_base_id_set(context, sender_id);
  }
}

static void utc_time_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;
  (void)len;
  sbp2nmea(context, msg, SBP2NMEA_SBP_UTC_TIME);
}

static void age_corrections_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;
  (void)len;
  sbp2nmea(context, msg, SBP2NMEA_SBP_AGE_CORR);
}

static void dops_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;
  (void)len;
  sbp2nmea(context, msg, SBP2NMEA_SBP_DOPS);
}

static void pos_llh_cov_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;
  (void)len;
  sbp2nmea(context, msg, SBP2NMEA_SBP_POS_LLH_COV);
}

static void vel_ned_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;
  (void)len;
  sbp2nmea(context, msg, SBP2NMEA_SBP_VEL_NED);
}

static int cleanup(int status);

int main(int argc, char *argv[])
{
  pk_settings_ctx_t *settings_ctx = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(cleanup(EXIT_FAILURE));
  }

  /* Need to init the converter before we get SBP in */
  sbp2nmea_t state = {0};
  sbp2nmea_init(&state, nmea_callback);

  if (sbp_init() != 0) {
    piksi_log(LOG_ERR, "error initializing SBP");
    exit(cleanup(EXIT_FAILURE));
  }

  nmea_pub = pk_endpoint_create(pk_endpoint_config()
                                  .endpoint(NMEA_PUB_ENDPOINT)
                                  .identity(NMEA_METRIC_NAME)
                                  .type(PK_ENDPOINT_PUB)
                                  .get());
  if (nmea_pub == NULL) {
    piksi_log(LOG_ERR, "error creating PUB socket");
    exit(cleanup(EXIT_FAILURE));
  }

  if (sbp_callback_register(SBP_MSG_OBS, msg_obs_callback, &state) != 0) {
    piksi_log(LOG_ERR, "error setting MSG OBS callback");
    exit(cleanup(EXIT_FAILURE));
  }

  if (sbp_callback_register(SBP_MSG_GPS_TIME, gps_time_callback, &state) != 0) {
    piksi_log(LOG_ERR, "error setting GPS TIME callback");
    exit(cleanup(EXIT_FAILURE));
  }

  if (sbp_callback_register(SBP_MSG_UTC_TIME, utc_time_callback, &state) != 0) {
    piksi_log(LOG_ERR, "error setting UTC TIME callback");
    return cleanup(EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_DOPS, dops_callback, &state) != 0) {
    piksi_log(LOG_ERR, "error setting dops callback");
    return cleanup(EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_AGE_CORRECTIONS, age_corrections_callback, &state) != 0) {
    piksi_log(LOG_ERR, "error setting Age of Corrections callback");
    return cleanup(EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_POS_LLH_COV, pos_llh_cov_callback, &state) != 0) {
    piksi_log(LOG_ERR, "error setting pos llh cov callback");
    return cleanup(EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_VEL_NED, vel_ned_callback, &state) != 0) {
    piksi_log(LOG_ERR, "error setting vel NED callback");
    return cleanup(EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_BASELINE_HEADING, baseline_heading_callback, &state) != 0) {
    piksi_log(LOG_ERR, "error setting baseline heading callback");
    return cleanup(EXIT_FAILURE);
  }

  settings_ctx = sbp_get_settings_ctx();

  pk_settings_register(settings_ctx,
                       "nmea",
                       "gpgga_msg_rate",
                       &gpgga_rate,
                       sizeof(gpgga_rate),
                       SETTINGS_TYPE_INT,
                       notify_gpgga_rate_changed,
                       &state);

  pk_settings_register(settings_ctx,
                       "nmea",
                       "gprmc_msg_rate",
                       &gprmc_rate,
                       sizeof(gprmc_rate),
                       SETTINGS_TYPE_INT,
                       notify_gprmc_rate_changed,
                       &state);

  pk_settings_register(settings_ctx,
                       "nmea",
                       "gpvtg_msg_rate",
                       &gpvtg_rate,
                       sizeof(gpvtg_rate),
                       SETTINGS_TYPE_INT,
                       notify_gpvtg_rate_changed,
                       &state);

  pk_settings_register(settings_ctx,
                       "nmea",
                       "gphdt_msg_rate",
                       &gphdt_rate,
                       sizeof(gphdt_rate),
                       SETTINGS_TYPE_INT,
                       notify_gphdt_rate_changed,
                       &state);

  pk_settings_register(settings_ctx,
                       "nmea",
                       "gpgll_msg_rate",
                       &gpgll_rate,
                       sizeof(gpgll_rate),
                       SETTINGS_TYPE_INT,
                       notify_gpgll_rate_changed,
                       &state);

  pk_settings_register(settings_ctx,
                       "nmea",
                       "gpzda_msg_rate",
                       &gpzda_rate,
                       sizeof(gpzda_rate),
                       SETTINGS_TYPE_INT,
                       notify_gpzda_rate_changed,
                       &state);

  pk_settings_register(settings_ctx,
                       "nmea",
                       "gsa_msg_rate",
                       &gsa_rate,
                       sizeof(gsa_rate),
                       SETTINGS_TYPE_INT,
                       notify_gsa_rate_changed,
                       &state);

  pk_settings_register_watch(settings_ctx,
                             "solution",
                             "soln_freq",
                             &soln_freq,
                             sizeof(soln_freq),
                             SETTINGS_TYPE_FLOAT,
                             notify_soln_freq_changed,
                             &state);

  pk_settings_register_watch(settings_ctx,
                             "nmea",
                             "gpgst_msg_rate",
                             &gpgst_rate,
                             sizeof(gpgst_rate),
                             SETTINGS_TYPE_INT,
                             notify_gpgst_rate_changed,
                             &state);

  sbp_run();

  exit(cleanup(EXIT_SUCCESS));
}

static int cleanup(int status)
{
  pk_endpoint_destroy(&nmea_pub);
  sbp_deinit();
  logging_deinit();
  return status;
}
