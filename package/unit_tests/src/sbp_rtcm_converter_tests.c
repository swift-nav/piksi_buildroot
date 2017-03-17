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

#include <sbp_rtcm3_bridge/sbp_rtcm3.h>
#include <sbp_rtcm3_bridge/rtcm3_messages.h>

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

  msg1001.sats[2].svId = 8;
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
  msg1001_out.header.msg_num = 1001;
  msg1001_out.header.stn_id = msg1001.header.stn_id;
  msg1001_out.header.div_free = msg1001.header.div_free;
  msg1001_out.header.smooth = msg1001.header.smooth;
  msg1001_out.header.sync = msg1001.header.sync;

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

  rtcm_obs_message msg1002_out = msg1001_out;
  sbp_to_rtcm3_obs( &sbp_obs, &msg1002_out );
  msg1002_out.header.msg_num = 1002;

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

  rtcm_obs_message msg1003_out = msg1001_out;
  sbp_to_rtcm3_obs( &sbp_obs, &msg1003_out );
  msg1003_out.header.msg_num = 1003;

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

  rtcm_obs_message msg1004_out = msg1001_out;
  sbp_to_rtcm3_obs( &sbp_obs, &msg1004_out );
  msg1004_out.header.msg_num = 1004;

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
  msg1005_out.stn_id = msg1005.stn_id;
  msg1005_out.ITRF = msg1005.ITRF;
  msg1005_out.GPS_ind = msg1005.GPS_ind;
  msg1005_out.GLO_ind = msg1005.GLO_ind;
  msg1005_out.GAL_ind = msg1005.GAL_ind;
  msg1005_out.ref_stn_ind = msg1005.ref_stn_ind;
  msg1005_out.osc_ind = msg1005.osc_ind;
  msg1005_out.quart_cycle_ind = msg1005.quart_cycle_ind;

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
  msg1006_out.msg_1005.stn_id = 5;
  msg1006_out.msg_1005.ref_stn_ind = 0;
  msg1006_out.msg_1005.quart_cycle_ind = 0;
  msg1006_out.msg_1005.osc_ind = 1;
  msg1006_out.msg_1005.ITRF = 0;
  msg1006_out.msg_1005.GPS_ind = 0;
  msg1006_out.msg_1005.GLO_ind = 0;
  msg1006_out.msg_1005.GAL_ind = 1;

  rtcm_msg_1006 msg1006_expected = msg1006;
  msg1006_expected.msg_1005.arp_x = 3573347.3347;
  msg1006_expected.msg_1005.arp_y = -5576347.7863;
  msg1006_expected.msg_1005.arp_z = 2578377.2472;
  msg1006_expected.ant_height = 0.0;

  sbp_to_rtcm3_1006( &sbp_base_pos, &msg1006_out );

  assert( msg1006_equals( &msg1006_expected, &msg1006_out ) );
}

// pgrgich: code snippet that casts incoming SBP message to a struct so it can be viewed in the debugger.
//    uint16_t sbp_message = ( frame[2] << 8 ) | frame[1];
//
//    msg_obs_t *obs;
//    msg_base_pos_ecef_t *position;
//    uint16_t obsid = SBP_MSG_OBS;
//    uint16_t posid = SBP_MSG_BASE_POS_ECEF;
//    if( sbp_message == SBP_MSG_OBS ) {
//        obs = (msg_obs_t*)(frame+6);
//        FILE *temp = fopen( "/home/pgrgich/out2.txt", "w" );
//        for( u8 obnum = 0; obnum < obs->header.n_obs; ++obnum ) {
//            packed_obs_content_t *ob = &obs->obs[ obnum ];
//            fprintf( temp, "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", ob->P, ob->L.i, ob->L.f, ob->D.i, ob->D.f, ob->cn0, ob->lock, ob->flags, ob->sid.sat, ob->sid.code );
//        }
//        fclose( temp );
//    }
//    else if( sbp_message == SBP_MSG_BASE_POS_ECEF ) {
//        position = (msg_base_pos_ecef_t*)(frame+6);
//    }
//    return;