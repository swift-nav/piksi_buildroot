#ifndef DEVICE_AND_PRODUCT_INFO_H
#define DEVICE_AND_PRODUCT_INFO_H

#include <string>

bool get_manufacturers_model_id(const size_t len, char *manufacturers_model_id);

bool get_manufacturers_model_serial_code(const size_t len,
                                         char *manufacturers_model_serial_code);

#endif  // DEVICE_AND_PRODUCT_INFO_H
