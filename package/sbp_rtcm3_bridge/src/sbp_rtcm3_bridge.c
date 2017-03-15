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

#include <stdint.h>
#include <czmq.h>
#include "sbp.h"
#include "sbp_rtcm3.h"
#include <rtcm3_io/src/rtcm3_decode.h>

#define RTCM3_SUB_ENDPOINT  ">tcp://127.0.0.1:45010"
#define SBP_PUB_ENDPOINT    ">tcp://127.0.0.1:43031"

static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("error opening %s\n", filename);
    return -1;
  }

  bool success = (fgets(str, str_size, fp) != NULL);

  fclose(fp);

  if (!success) {
    printf("error reading %s\n", filename);
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  rtcm_msg_header header;
  header.msg_num = 1001;
  header.div_free = 0;
  header.n_sat = 3;
  header.smooth = 0;
  header.stn_id = 7;
  header.sync = 1;
  header.tow = 309000;

  rtcm_obs_message msg1001;
  memset( (void*)&msg1001, 0, sizeof( msg1001 ) );
  msg1001.header = header;
  msg1001.sats[0].svId = 4;
  msg1001.sats[0].obs[0].code = 0;
  msg1001.sats[0].obs[0].pseudorange = 20000004.4;
  msg1001.sats[0].obs[0].carrier_phase = 105100794.4;
  msg1001.sats[0].obs[0].lock = 900;
  msg1001.sats[0].obs[0].flags.valid_pr = 1;
  msg1001.sats[0].obs[0].flags.valid_cp = 1;
  msg1001.sats[0].obs[0].flags.valid_lock = 1;

  msg1001.sats[1].svId = 6;
  msg1001.sats[1].obs[0].code = 0;
  msg1001.sats[1].obs[0].pseudorange = 22000004.4;
  msg1001.sats[1].obs[0].carrier_phase = 110000005.4;
  msg1001.sats[1].obs[0].lock = 254;
  msg1001.sats[1].obs[0].flags.valid_pr = 1;
  msg1001.sats[1].obs[0].flags.valid_cp = 1;
  msg1001.sats[1].obs[0].flags.valid_lock = 1;

  msg1001.sats[2].svId = 6;
  msg1001.sats[2].obs[0].code = 0;
  msg1001.sats[2].obs[0].pseudorange = 22000004.4;
  msg1001.sats[2].obs[0].carrier_phase = 110000005.4;
  msg1001.sats[2].obs[0].lock = 254;
  msg1001.sats[2].obs[0].flags.valid_pr = 1;
  msg1001.sats[2].obs[0].flags.valid_cp = 0;
  msg1001.sats[2].obs[0].flags.valid_lock = 0;

  msg_obs_t sbp_obs;
  rtcm3_obs_to_sbp(&msg1001, &sbp_obs );

  rtcm_obs_message msg1001_out;
  sbp_to_rtcm3_obs( &sbp_obs, &msg1001_out );

  assert( msgobs_equals( &msg1001, &msg1001_out ) );

  rtcm_obs_message msg1002;
  msg1002 = msg1001;
  msg1002.header.msg_num = 1002;
  msg1002.sats[0].obs[0].cnr = 3.4;
  msg1002.sats[0].obs[0].flags.valid_cnr = 1;

  msg1002.sats[1].obs[0].cnr = 50.2;
  msg1002.sats[1].obs[0].flags.valid_cnr = 1;

  msg1002.sats[2].obs[0].cnr = 50.2;
  msg1002.sats[2].obs[0].flags.valid_cnr = 0;

  rtcm3_obs_to_sbp(&msg1002, &sbp_obs );

  rtcm_obs_message msg1002_out;
  sbp_to_rtcm3_obs( &sbp_obs, &msg1002_out );

  assert( msgobs_equals( &msg1002, &msg1002_out ) );

  rtcm_obs_message msg1003;
  msg1003 = msg1001;
  msg1003.header.msg_num = 1003;

  msg1003.sats[0].obs[1] = msg1003.sats[0].obs[0];
  msg1003.sats[0].obs[1].pseudorange = 20000124.4;
  msg1003.sats[0].obs[1].carrier_phase = 81897184.4;

  msg1003.sats[1].obs[1] = msg1003.sats[1].obs[0];
  msg1003.sats[1].obs[1].pseudorange = 22000024.4;
  msg1003.sats[1].obs[1].carrier_phase = 90086422.236;

  rtcm3_obs_to_sbp(&msg1003, &sbp_obs );

  rtcm_obs_message msg1003_out;
  sbp_to_rtcm3_obs( &sbp_obs, &msg1003_out );

  assert( msgobs_equals( &msg1003, &msg1003_out ) );

  rtcm_obs_message msg1004;
  msg1004 = msg1003;
  msg1004.header.msg_num = 1004;

  msg1004.sats[0].obs[0].cnr = 3.4;
  msg1004.sats[0].obs[0].flags.valid_cnr = 1;
  msg1004.sats[0].obs[1].cnr = 1.4;
  msg1004.sats[0].obs[1].flags.valid_cnr = 1;

  msg1004.sats[1].obs[0].cnr = 50.2;
  msg1004.sats[1].obs[0].flags.valid_cnr = 1;
  msg1004.sats[1].obs[1].cnr = 50.2;
  msg1004.sats[1].obs[1].flags.valid_cnr = 1;

  msg1004.sats[2].obs[0].cnr = 50.2;
  msg1004.sats[2].obs[0].flags.valid_cnr = 0;
  msg1004.sats[2].obs[1].cnr = 54.2;
  msg1004.sats[2].obs[1].flags.valid_cnr = 1;

  rtcm3_obs_to_sbp(&msg1004, &sbp_obs );

  rtcm_obs_message msg1004_out;
  sbp_to_rtcm3_obs( &sbp_obs, &msg1004_out );

  assert( msgobs_equals( &msg1004, &msg1004_out ) );

  rtcm_msg_1005 msg1005;

  msg1005.stn_id = 5;
  msg1005.ref_stn_ind = 1;
  msg1005.quart_cycle_ind = 1;
  msg1005.osc_ind = 0;
  msg1005.ITRF = 1;
  msg1005.GPS_ind = 1;
  msg1005.GLO_ind = 1;
  msg1005.GAL_ind = 0;
  msg1005.arp_x = 3578346.5475;
  msg1005.arp_y = -5578346.5578;
  msg1005.arp_z = 2578346.6757;

  msg_base_pos_ecef_t sbp_base_pos;
  rtcm3_1005_to_sbp(&msg1005, &sbp_base_pos );

  rtcm_msg_1005 msg1005_out;
  sbp_to_rtcm3_1005( &sbp_base_pos, &msg1005_out );

  assert( msg1005_equals( &msg1005, &msg1005_out ) );

  rtcm_msg_1006 msg1006;

  msg1006.msg_1005.stn_id = 5;
  msg1006.msg_1005.ref_stn_ind = 0;
  msg1006.msg_1005.quart_cycle_ind = 0;
  msg1006.msg_1005.osc_ind = 1;
  msg1006.msg_1005.ITRF = 0;
  msg1006.msg_1005.GPS_ind = 0;
  msg1006.msg_1005.GLO_ind = 0;
  msg1006.msg_1005.GAL_ind = 1;
  msg1006.msg_1005.arp_x = 3573346.5475;
  msg1006.msg_1005.arp_y = -5576346.5578;
  msg1006.msg_1005.arp_z = 2578376.6757;
  msg1006.ant_height = 1.567;

  rtcm3_1006_to_sbp(&msg1006, &sbp_base_pos );

  rtcm_msg_1006 msg1006_out;
  sbp_to_rtcm3_1006( &sbp_base_pos, &msg1006_out );

  assert( msg1006_equals( &msg1006, &msg1006_out ) );


  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  /* Read SBP sender ID */
  u16 sbp_sender_id = SBP_SENDER_ID;
  char sbp_sender_id_string[32];
  if (file_read_string("/cfg/sbp_sender_id", sbp_sender_id_string,
                        sizeof(sbp_sender_id_string)) == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }

  if (sbp_init(sbp_sender_id, SBP_PUB_ENDPOINT) != 0) {
    exit(EXIT_FAILURE);
  }

  zsock_t *rtcm3_sub = zsock_new_sub(RTCM3_SUB_ENDPOINT, "");
  if (rtcm3_sub == NULL) {
    printf("error creating SUB socket\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    zmsg_t *msg = zmsg_recv(rtcm3_sub);
    if (msg == NULL) {
      continue;
    }

    zframe_t *frame;
    for (frame = zmsg_first(msg); frame != NULL; frame = zmsg_next(msg)) {
      rtcm3_decode_frame(zframe_data(frame), zframe_size(frame));
    }

    zmsg_destroy(&msg);
  }

  exit(EXIT_SUCCESS);
}
