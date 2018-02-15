#ifndef SBPTONMEA2KCONVERTER_H
#define SBPTONMEA2KCONVERTER_H

#include <N2kMessages.h>

extern "C" {
#include <libsbp/navigation.h>
#include <libsbp/orientation.h>
#include <libsbp/tracking.h>
}

class SbpToNmea2kConverter {
public:
    bool Sbp65ToPgn129540(const msg_tracking_state_t *msg, const u8 len,
                          tN2kMsg *n2kMsg);

private:
    u8 last_sid = 0;

    u8 tow_to_sid(const u32 tow);
};


#endif  // SBPTONMEA2KCONVERTER_H
