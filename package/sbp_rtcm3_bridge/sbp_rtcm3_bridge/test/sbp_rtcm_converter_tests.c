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

#include "sbp_rtcm_converter_tests.h"
#include "rtcm_decoder_tests.h"
#include <assert.h>
#include <rtcm3_messages.h>
#include <sbp_rtcm3.h>
#include <string.h>

void test_sbp_rtcm_converter() {
  rtcm_msg_header header;
  header.msg_num = 1001;
  header.div_free = 0;
  header.n_sat = 3;
  header.smooth = 0;
  header.stn_id = 7;
  header.sync = 1;
  header.tow = 309000;

  rtcm_obs_message msg1001;
  memset((void *)&msg1001, 0, sizeof(msg1001));
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

  msg1001.sats[2].svId = 8;
  msg1001.sats[2].obs[0].code = 0;
  msg1001.sats[2].obs[0].pseudorange = 22000004.4;
  msg1001.sats[2].obs[0].carrier_phase = 110000005.4;
  msg1001.sats[2].obs[0].lock = 254;
  msg1001.sats[2].obs[0].flags.valid_pr = 1;
  msg1001.sats[2].obs[0].flags.valid_cp = 0;
  msg1001.sats[2].obs[0].flags.valid_lock = 0;

  u8 obs_data[4 * sizeof(observation_header_t) +
              4 * MAX_OBS_IN_SBP * sizeof(packed_obs_content_t)];
  msg_obs_t *sbp_obs[4];
  for (u8 msg = 0; msg < 4; ++msg) {
    sbp_obs[msg] =
        (msg_obs_t *)(obs_data +
                      (msg * sizeof(observation_header_t) +
                       MAX_OBS_IN_SBP * msg * sizeof(packed_obs_content_t)));
  }

  u8 sizes[4];
  u8 num_msgs = rtcm3_obs_to_sbp(&msg1001, sbp_obs, sizes);

  rtcm_obs_message msg1001_out;
  msg1001_out.header.n_sat = 0;
  for (u8 msg = 0; msg < num_msgs; ++msg) {
    sbp_to_rtcm3_obs(sbp_obs[msg], sizes[msg], &msg1001_out);
  }

  msg1001_out.header.msg_num = 1001;
  msg1001_out.header.stn_id = msg1001.header.stn_id;
  msg1001_out.header.div_free = msg1001.header.div_free;
  msg1001_out.header.smooth = msg1001.header.smooth;
  msg1001_out.header.sync = msg1001.header.sync;

  /* TODO: fix me */
  /* assert(msgobs_equals(&msg1001, &msg1001_out)); */

  rtcm_obs_message msg1002;
  msg1002 = msg1001;
  msg1002.header.msg_num = 1002;
  msg1002.sats[0].obs[0].cnr = 3.4;
  msg1002.sats[0].obs[0].flags.valid_cnr = 1;

  msg1002.sats[1].obs[0].cnr = 50.2;
  msg1002.sats[1].obs[0].flags.valid_cnr = 1;

  msg1002.sats[2].obs[0].cnr = 50.2;
  msg1002.sats[2].obs[0].flags.valid_cnr = 0;

  num_msgs = rtcm3_obs_to_sbp(&msg1002, sbp_obs, sizes);

  rtcm_obs_message msg1002_out = msg1001_out;
  msg1002_out.header.n_sat = 0;
  for (u8 msg = 0; msg < num_msgs; ++msg) {
    sbp_to_rtcm3_obs(sbp_obs[msg], sizes[msg], &msg1002_out);
  }

  msg1002_out.header.msg_num = 1002;

  /* TODO: fix me */
  /* assert(msgobs_equals(&msg1002, &msg1002_out)); */

  rtcm_obs_message msg1003;
  msg1003 = msg1001;
  msg1003.header.msg_num = 1003;

  msg1003.sats[0].obs[1] = msg1003.sats[0].obs[0];
  msg1003.sats[0].obs[1].pseudorange = 20000124.4;
  msg1003.sats[0].obs[1].carrier_phase = 81897184.4;

  msg1003.sats[1].obs[1] = msg1003.sats[1].obs[0];
  msg1003.sats[1].obs[1].pseudorange = 22000024.4;
  msg1003.sats[1].obs[1].carrier_phase = 90086422.236;

  num_msgs = rtcm3_obs_to_sbp(&msg1003, sbp_obs, sizes);

  rtcm_obs_message msg1003_out = msg1001_out;
  msg1003_out.header.n_sat = 0;
  for (u8 msg = 0; msg < num_msgs; ++msg) {
    sbp_to_rtcm3_obs(sbp_obs[msg], sizes[msg], &msg1003_out);
  }

  msg1003_out.header.msg_num = 1003;

  assert(msgobs_equals(&msg1003, &msg1003_out));

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

  msg1004.sats[3] = msg1004.sats[0];
  msg1004.sats[3].svId = 10;
  msg1004.sats[4] = msg1004.sats[0];
  msg1004.sats[4].svId = 11;
  msg1004.sats[5] = msg1004.sats[0];
  msg1004.sats[5].svId = 12;
  msg1004.sats[6] = msg1004.sats[0];
  msg1004.sats[6].svId = 13;
  msg1004.sats[7] = msg1004.sats[0];
  msg1004.sats[7].svId = 14;
  msg1004.sats[8] = msg1004.sats[0];
  msg1004.sats[8].svId = 15;
  msg1004.sats[9] = msg1004.sats[0];
  msg1004.sats[9].svId = 16;
  msg1004.sats[10] = msg1004.sats[0];
  msg1004.sats[10].svId = 17;
  msg1004.header.n_sat = 11;

  num_msgs = rtcm3_obs_to_sbp(&msg1004, sbp_obs, sizes);

  rtcm_obs_message msg1004_out = msg1001_out;
  msg1004_out.header.n_sat = 0;
  for (u8 msg = 0; msg < num_msgs; ++msg) {
    sbp_to_rtcm3_obs(sbp_obs[msg], sizes[msg], &msg1004_out);
  }

  msg1004_out.header.msg_num = 1004;

  assert(msgobs_equals(&msg1004, &msg1004_out));

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
  rtcm3_1005_to_sbp(&msg1005, &sbp_base_pos);

  rtcm_msg_1005 msg1005_out;
  msg1005_out.stn_id = msg1005.stn_id;
  msg1005_out.ITRF = msg1005.ITRF;
  msg1005_out.GPS_ind = msg1005.GPS_ind;
  msg1005_out.GLO_ind = msg1005.GLO_ind;
  msg1005_out.GAL_ind = msg1005.GAL_ind;
  msg1005_out.ref_stn_ind = msg1005.ref_stn_ind;
  msg1005_out.osc_ind = msg1005.osc_ind;
  msg1005_out.quart_cycle_ind = msg1005.quart_cycle_ind;

  sbp_to_rtcm3_1005(&sbp_base_pos, &msg1005_out);

  assert(msg1005_equals(&msg1005, &msg1005_out));

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

  rtcm3_1006_to_sbp(&msg1006, &sbp_base_pos);

  rtcm_msg_1006 msg1006_out;
  msg1006_out.msg_1005.stn_id = 5;
  msg1006_out.msg_1005.ref_stn_ind = 0;
  msg1006_out.msg_1005.quart_cycle_ind = 0;
  msg1006_out.msg_1005.osc_ind = 1;
  msg1006_out.msg_1005.ITRF = 0;
  msg1006_out.msg_1005.GPS_ind = 0;
  msg1006_out.msg_1005.GLO_ind = 0;
  msg1006_out.msg_1005.GAL_ind = 1;

  rtcm_msg_1006 msg1006_expected = msg1006;
  msg1006_expected.msg_1005.arp_x = 3573346.5475;
  msg1006_expected.msg_1005.arp_y = -5576346.5578;
  msg1006_expected.msg_1005.arp_z = 2578376.6757;
  msg1006_expected.ant_height = 0.0;

  sbp_to_rtcm3_1006(&sbp_base_pos, &msg1006_out);

  assert(msg1006_equals(&msg1006_expected, &msg1006_out));
}
