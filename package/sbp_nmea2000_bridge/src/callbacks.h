#ifndef SBP_NMEA2000_BRIDGE_CALLBACKS_H
#define SBP_NMEA2000_BRIDGE_CALLBACKS_H

#include "sbp.h"

// The callbacks do the SBP -> NMEA2000 conversion and send out the
// NMEA2000 messages after conversion.

int callback_can_debug(zloop_t *loop, zmq_pollitem_t *item,
                       void *interface_name_void);

// PGN 126992
void callback_sbp_utc_time(u16 sender_id, u8 len, u8 msg[], void *context);

// PGN 127250
void callback_sbp_baseline_heading(u16 sender_id, u8 len, u8 msg[],
                                   void *context);

// PGN 129025
void callback_sbp_pos_llh(u16 sender_id, u8 len, u8 msg[], void *context);

// PGN 129026
void callback_sbp_vel_ned(u16 sender_id, u8 len, u8 msg[], void *context);

// PGN 129539
void callback_sbp_dops(u16 sender_id, u8 len, u8 msg[], void *context);

// PGN 129540
void callback_sbp_tracking_state(u16 sender_id, u8 len, u8 msg[],
                                 void *context);

void callback_sbp_heartbeat(u16 sender_id, u8 len, u8 msg[], void *context);

#endif //SBP_NMEA2000_BRIDGE_CALLBACKS_H
