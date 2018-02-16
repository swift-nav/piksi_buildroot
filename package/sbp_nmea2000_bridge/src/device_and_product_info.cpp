#include <fstream>

#include "device_and_product_info.h"

namespace {
    constexpr char cEepromPath[] =
            "/sys/devices/soc0/amba/e0005000.i2c/i2c-1/1-0050/eeprom";
    constexpr char cManufacturersModelSerialCodePath[] = "/factory/mfg_id";
}  // namespace

bool get_manufacturers_model_id(const size_t len,
                                char *manufacturers_model_id) {
  constexpr size_t cMinLengthNeeded = 6;
  if(cMinLengthNeeded > len) {
    return false;
  }

  std::fstream eeprom_file(cEepromPath, std::ios_base::in);
  eeprom_file.get(manufacturers_model_id, cMinLengthNeeded);

  if (std::string(manufacturers_model_id, 4) == "DURO") {
    manufacturers_model_id[4] = '\0';
  } else if (std::string(manufacturers_model_id) != "PIKSI") {
    return false;
  }

  return true;
}

bool
get_manufacturers_model_serial_code(const size_t len,
                                    char *manufacturers_model_serial_code) {
  std::fstream manufacturers_model_serial_code_file(
          cManufacturersModelSerialCodePath,
          std::ios_base::in);
  manufacturers_model_serial_code_file.get(manufacturers_model_serial_code,
                                           len);
  return true;
}