#ifndef SBP_NMEA2000_BRIDGE_COMMON_H
#define SBP_NMEA2000_BRIDGE_COMMON_H

#include <iostream>

#include <NMEA2000.h>

#define UNUSED(x) (void)(x)

void piksi_check(int err, const char* format, ...);

extern bool debug;

struct debug_stream {
    template<typename T>
    debug_stream& operator<<(T arg) {
      if (debug) {
        std::cout << std::dec << arg;
      }
      return *this;
    }
};
extern debug_stream d;

extern tNMEA2000 &NMEA2000;

#endif // SBP_NMEA2000_BRIDGE_COMMON_H
