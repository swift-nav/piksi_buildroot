#include <cstdarg>
#include <iomanip>

extern "C" {
#include <libpiksi/logging.h>
}

#include "common.h"
#include "NMEA2000_CAN.h"

void piksi_check(int err, const char* format, ...) {
  if (err != 0) {
    va_list ap;
    va_start(ap, format);
    piksi_log(LOG_ERR, format, ap);
    if(debug) {
      vprintf(format, ap);
      printf("\n");
    }
    va_end(ap);
    exit(EXIT_FAILURE);
  }
}

bool debug = false;

debug_stream d;