#include <algorithm>
#include <iomanip>
#include <iostream>
#include <linux/can.h>

#include <N2kMessages.h>

extern "C" {
#include <libsbp/navigation.h>
#include <libsbp/orientation.h>
#include <libsbp/tracking.h>
}

#include "callbacks.h"
#include "common.h"
#include "SbpToNmea2kConverter.h"

namespace {
    SbpToNmea2kConverter converter;
}

bool setting_sbp_tracking = true;
bool setting_sbp_utc = true;
bool setting_sbp_heading = true;
bool setting_sbp_llh = true;
bool setting_sbp_vel_ned = true;
bool setting_sbp_dops = true;
bool setting_sbp_heartbeat = true;

bool setting_n2k_126992 = true;
bool setting_n2k_127250 = true;
bool setting_n2k_129025 = true;
bool setting_n2k_129026 = true;
bool setting_n2k_129029 = true;
bool setting_n2k_129539 = true;
bool setting_n2k_129540 = true;

int callback_can_debug(zloop_t *loop, zmq_pollitem_t *item,
                       void *interface_name_void) {
  UNUSED(loop);
  auto interface_name = reinterpret_cast<const char*>(interface_name_void);

  canfd_frame frame;
  ssize_t bytes_read = recvfrom(item->fd, &frame,sizeof(frame),
          /*flags=*/0, /*src_addr=*/NULL, /*addrlen=*/NULL);
  if(bytes_read < 0) {
    d << "Could not read bytes from " << interface_name << "\n";
    return 0;
  }

  d << "Read " << bytes_read << " bytes from " << interface_name <<" ID: "
    << std::setfill('0') << std::setw(4) << std::hex << frame.can_id
    << " Data: ";
  for (u32 i = 0; i < frame.len; ++i) {
    d << std::setfill('0') << std::setw(2) << std::hex
      << static_cast<u16>(frame.data[i]);
  }
  d << "\n";

  return 0;
}

// PGN 126992
void callback_sbp_utc_time(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  if (!setting_sbp_utc) {
    return;
  }

  auto sbp_utc_time = reinterpret_cast<msg_utc_time_t*>(msg);
  bool is_valid = (sbp_utc_time->flags & 0x07) != 0;

  tN2kMsg n2kMsg;
  u16 days_since_1970;
  double seconds_since_midnight;
  if (converter.Sbp259ToPgn126992(sbp_utc_time, &n2kMsg, &days_since_1970,
                                  &seconds_since_midnight) &&
      setting_n2k_126992) {
    NMEA2000.SendMsg(n2kMsg);
  }

  converter.ResetPgn129029CacheIfOld(sbp_utc_time->tow);
  converter.SetPgn129029CacheFields(sbp_utc_time->tow, days_since_1970,
                                    seconds_since_midnight);
  if (is_valid && converter.IsPgn129029Ready(/*is_valid=*/true)) {
    n2kMsg.Clear();
    if (converter.Sbp259And520And522ToPgn129029(/*is_valid=*/true, &n2kMsg) &&
        setting_n2k_129029) {
      NMEA2000.SendMsg(n2kMsg);
    }
  }
}

// PGN 127250
void callback_sbp_baseline_heading(u16 sender_id, u8 len, u8 msg[],
                                   void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  if (!setting_sbp_heading) {
    return;
  }

  tN2kMsg n2kMsg;
  if(converter.Sbp527ToPgn127250(reinterpret_cast<msg_baseline_heading_t *>(msg),
                                 &n2kMsg) && setting_n2k_127250) {
    NMEA2000.SendMsg(n2kMsg);
  }
}

// PGN 129025
void callback_sbp_pos_llh(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  if (!setting_sbp_llh) {
    return;
  }

  auto *sbp_pos = reinterpret_cast<msg_pos_llh_t*>(msg);
  bool is_valid = (sbp_pos->flags & 0x07) != 0;

  tN2kMsg n2kMsg;
  if (converter.Sbp522ToPgn129025(sbp_pos, &n2kMsg) && setting_n2k_129025) {
    NMEA2000.SendMsg(n2kMsg);
  }

  // In case of invalid position, pos_llh message marks an epoch.
  // Therefore, we don't check for validity here.
  converter.ResetPgn129029CacheIfOld(sbp_pos->tow);
  converter.SetPgn129029CacheFields(sbp_pos);
  if (converter.IsPgn129029Ready(is_valid)) {
    n2kMsg.Clear();
    if (converter.Sbp259And520And522ToPgn129029(is_valid, &n2kMsg) &&
        setting_n2k_129029) {
      NMEA2000.SendMsg(n2kMsg);
    }
  }
}

// PGN 129026
void callback_sbp_vel_ned(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  if (!setting_sbp_vel_ned) {
    return;
  }

  tN2kMsg n2kMsg;
  if (converter.Sbp526ToPgn129026(reinterpret_cast<msg_vel_ned_t*>(msg),
                                  &n2kMsg) && setting_n2k_129026) {
    NMEA2000.SendMsg(n2kMsg);
  }
}

// PGN 129539
void callback_sbp_dops(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(context);

  if (!setting_sbp_dops) {
    return;
  }

  auto sbp_dops = reinterpret_cast<msg_dops_t*>(msg);
  bool is_valid = (sbp_dops->flags & 0x07) != 0;

  tN2kMsg n2kMsg;
  if (converter.Sbp520ToPgn129539(sbp_dops, &n2kMsg) && setting_n2k_129539) {
    NMEA2000.SendMsg(n2kMsg);
  }

  converter.ResetPgn129029CacheIfOld(sbp_dops->tow);
  converter.SetPgn129029CacheFields(sbp_dops);
  if (is_valid &&
      converter.IsPgn129029Ready(/*is_valid=*/true)) {
    n2kMsg.Clear();
    if (converter.Sbp259And520And522ToPgn129029(/*is_valid=*/true, &n2kMsg) &&
        setting_n2k_129029) {
      NMEA2000.SendMsg(n2kMsg);
    }
  }
}

// PGN 129540
void callback_sbp_tracking_state(u16 sender_id, u8 len, u8 msg[],
                                 void *context) {
  UNUSED(sender_id);
  UNUSED(context);

  if (!setting_sbp_tracking) {
    return;
  }

  tN2kMsg n2kMsg;
  if (converter.Sbp65ToPgn129540(reinterpret_cast<msg_tracking_state_t*>(msg),
                                len, &n2kMsg) && setting_n2k_129540) {
    NMEA2000.SendMsg(n2kMsg);
  }
}

void callback_sbp_heartbeat(u16 sender_id, u8 len, u8 msg[], void *context) {
  UNUSED(sender_id);
  UNUSED(len);
  UNUSED(msg);
  UNUSED(context);

  if (!setting_sbp_heartbeat) {
    return;
  }

  d << __FUNCTION__ << "\n";

  // This checks for new CAN messages and sends out a heartbeat.
  NMEA2000.ParseMessages();

  d << "\tDone.\n";
}