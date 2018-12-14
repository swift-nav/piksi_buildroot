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

#include <float.h>
#include <getopt.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings_client.h>
#include <libpiksi/util.h>

#include <libsbp/navigation.h>
#include <libsbp/sbp.h>
#include <libsbp/system.h>

#include "sbp.h"
#include "nav_daemon.h"

#define PROGRAM_NAME "nav_daemon"

#define DEBUG 0

static bool smoothpose_enable;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMain options");
  puts("\t--test <test option>");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_TEST = 1
  };

  const struct option long_opts[] = {
    {"TEST", required_argument, 0, OPT_ID_TEST},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case OPT_ID_TEST: {
        int test = atof(optarg);
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


static void pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void*) context;
  msg_pos_llh_t *pos = (msg_pos_llh_t*)msg;

  static time_t last_log_msg_time = 0;
  time_t now = time(NULL);

  if (last_log_msg_time == 0 || now - last_log_msg_time >= 1L) {

    sbp_log(LOG_DEBUG, "SBP_MSG_POS_LLH: lat = %f lon = %f height = %f",
              pos->lat, pos->lon, pos->height);
    last_log_msg_time = time(NULL);
  }

  if (smoothpose_enable) {
    sbp_tx_send(NULL, SBP_MSG_POS_LLH,
                  sizeof(*pos), (u8*) pos);
  }
}


static void pos_ecef_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void *) context;
  msg_pos_ecef_t *pos = (msg_pos_ecef_t*)msg;

  static time_t last_log_msg_time = 0;
  time_t now = time(NULL);

  if (last_log_msg_time == 0 || now - last_log_msg_time >= 1L) {

    sbp_log(LOG_DEBUG, "SBP_MSG_POS_ECEF: X = %f Y = %f Z = %f" ,
              pos->x, pos->y, pos->z);

    last_log_msg_time = time(NULL);
  }
  if (smoothpose_enable) {
    sbp_tx_send(NULL, SBP_MSG_POS_ECEF,
                  sizeof(*pos), (u8*) pos);
  }
}

static void vel_ned_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void*) context;
  msg_vel_ned_t *vel = (msg_vel_ned_t*)msg;

  static time_t last_log_msg_time = 0;
  time_t now = time(NULL);

  if (last_log_msg_time == 0 || now - last_log_msg_time >= 1L) {

    sbp_log(LOG_DEBUG, "SBP_MSG_VEL_NED: N = %f E = %f D = %f",
              vel->n, vel->e, vel->d);

    last_log_msg_time = time(NULL);
  }

  if (smoothpose_enable) {
    sbp_tx_send(NULL, SBP_MSG_VEL_NED,
                  sizeof(*vel), (u8*) vel);
  }
}

static void vel_ecef_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void*)  context;
  msg_vel_ecef_t *vel = (msg_vel_ecef_t*)msg;

  static time_t last_log_msg_time = 0;
  time_t now = time(NULL);

  if (last_log_msg_time == 0 || now - last_log_msg_time >= 1L) {

    sbp_log(LOG_DEBUG, "SBP_MSG_VEL_ECEF: X = %f Y = %f Z = %f",
              vel->x, vel->y, vel->z);

    last_log_msg_time = time(NULL);
  }

  if (smoothpose_enable) {
    sbp_tx_send(NULL, SBP_MSG_VEL_ECEF,
                  sizeof(*vel), (u8*) vel);
  }
}

static int notify_settings_changed(void *context)
{
  (void* )context;
  //log_warn("Enabling or Disabling Smoothpose requires device restart");
  return 0;
}

int main(int argc, char *argv[])
{
  int status = EXIT_SUCCESS;
  pk_settings_ctx_t *settings_ctx = NULL;
 
  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (sbp_init() != 0) {
    piksi_log(LOG_ERR | LOG_SBP, "Error initializing SBP!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  settings_ctx = sbp_get_settings_ctx();

  if (sbp_callback_register(SBP_MSG_POS_LLH, pos_llh_callback, NULL) != 0) {
    sbp_log(LOG_ERR, "Error setting MSG_POS_LLH callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }
  
  if (sbp_callback_register(SBP_MSG_POS_ECEF, pos_ecef_callback, NULL) != 0) {
    sbp_log(LOG_ERR, "Error setting MSG_POS_ECEF callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }
  
  if (sbp_callback_register(SBP_MSG_VEL_NED, vel_ned_callback, NULL) != 0) {
    sbp_log(LOG_ERR, "Error setting MSG_VEL_NED callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }
  
  if (sbp_callback_register(SBP_MSG_VEL_ECEF, vel_ecef_callback, NULL) != 0) {
    sbp_log(LOG_ERR, "Error setting MSG_VEL_NED callback!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  if (settings_ctx == NULL) {
    sbp_log(LOG_ERR, "Error registering for settings!");
    status = EXIT_FAILURE;
    goto cleanup;
  }

  pk_settings_register(settings_ctx, "ins", "smoothpose_enable",
                    &smoothpose_enable, sizeof(smoothpose_enable),
                    SETTINGS_TYPE_BOOL,
                    notify_settings_changed, NULL);

  sbp_log(LOG_INFO, "Ready!");
  sbp_run();

cleanup:
  sbp_deinit();
  logging_deinit();
  return status;
}
