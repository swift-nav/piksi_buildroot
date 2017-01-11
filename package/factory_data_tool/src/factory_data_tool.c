/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include "uboot/factory_data.h"
#include "uboot/image_table.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static const struct {
  uint32_t hardware;
  const char *name;
} image_hardware_strings[] = {
  { IMAGE_HARDWARE_UNKNOWN,       "unknown"       },
  { IMAGE_HARDWARE_V3_MICROZED,   "v3_microzed"   },
  { IMAGE_HARDWARE_V3_EVT1,       "v3_evt1"       },
  { IMAGE_HARDWARE_V3_EVT2,       "v3_evt2"       },
  { IMAGE_HARDWARE_V3_PROD,       "v3_prod"       },
};

static const factory_data_t * factory_data_get(void)
{
  /* open file */
  int fd = open("/factory/mtd", O_RDONLY);
  if (fd < 0) {
    printf("error opening /factory/mtd\n");
    return NULL;
  }

  /* allocate buffer to hold header */
  factory_data_t *factory_data =
      (factory_data_t *)malloc(sizeof(factory_data_t));
  if (factory_data == NULL) {
    printf("error allocating buffer for factory data header\n");
    return NULL;
  }

  /* read header */
  if (read(fd, factory_data, sizeof(factory_data_t)) !=
          sizeof(factory_data_t)) {
    printf("error reading /factory/mtd\n");
    return NULL;
  }

  /* verify header */
  if (factory_data_header_verify(factory_data) != 0) {
    printf("error verifying factory data header\n");
    return NULL;
  }

  /* reallocate buffer to hold header + body */
  uint32_t factory_data_body_size = factory_data_body_size_get(factory_data);
  uint32_t factory_data_size = sizeof(factory_data_t) +
                               factory_data_body_size;
  factory_data = (factory_data_t *)realloc((void *)factory_data,
                                           factory_data_size);
  if (factory_data == NULL) {
    printf("error allocating buffer for factory data\n");
    return NULL;
  }

  /* read body */
  if (read(fd, &factory_data->body[0], factory_data_body_size) !=
          factory_data_body_size) {
    printf("error reading /factory/mtd\n");
    return NULL;
  }

  /* verify body */
  if (factory_data_body_verify(factory_data) != 0) {
    printf("error verifying factory data body\n");
    return NULL;
  }

  /* close file */
  close(fd);

  return factory_data;
}

static char nibble_to_char(uint8_t nibble)
{
  if (nibble < 10) {
    return '0' + nibble;
  } else if (nibble < 16) {
    return 'a' + nibble - 10;
  } else {
    return '?';
  }
}

static void print_hex_string(char *str, const uint8_t *data, uint32_t data_size)
{
  int i;
  for (i=0; i<data_size; i++) {
    str[2*i + 0] = nibble_to_char((data[data_size - 1 - i] >> 4) & 0xf);
    str[2*i + 1] = nibble_to_char((data[data_size - 1 - i] >> 0) & 0xf);
  }
  str[2*i] = 0;
}

static int factory_file_write(const char *filename, const char *data)
{
  /* generate file path */
  char filepath[256];
  snprintf(filepath, sizeof(filepath), "/factory/%s", filename);

  /* open file */
  int fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0) {
    printf("error opening %s\n", filepath);
    return -1;
  }

  /* write to file */
  size_t data_len = strlen(data);
  if (write(fd, data, data_len) != data_len) {
    printf("error writing %s\n", filepath);
  }

  /* close file */
  close(fd);

  return 0;
}

static int factory_file_write_u32(const char *filename, uint32_t data)
{
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%d", data);
  return factory_file_write(filename, buffer);
}

static int factory_file_write_hex_string(const char *filename,
                                         const uint8_t *data,
                                         uint32_t data_size)
{
  char buffer[2 * data_size + 1];
  print_hex_string(buffer, data, data_size);
  return factory_file_write(filename, buffer);
}

int main(int argc, char *argv[])
{
  const factory_data_t *factory_data = factory_data_get();
  if (factory_data == NULL) {
    exit(EXIT_FAILURE);
  }

  uint32_t hardware;
  if (factory_data_hardware_get(factory_data, &hardware) == 0) {
    factory_file_write_u32("hardware", hardware);

    int i;
    for (i=0; i<ARRAY_SIZE(image_hardware_strings); i++) {
      if (image_hardware_strings[i].hardware == hardware) {
        factory_file_write("hardware_name", image_hardware_strings[i].name);
        break;
      }
    }
  }

  uint8_t mfg_id[17 + 1];
  if (factory_data_mfg_id_get(factory_data, mfg_id) == 0) {
    mfg_id[sizeof(mfg_id) - 1] = 0;
    factory_file_write("mfg_id", mfg_id);
  }

  uint8_t uuid[16];
  if (factory_data_uuid_get(factory_data, uuid) == 0) {
    factory_file_write_hex_string("uuid", uuid, sizeof(uuid));
  }

  uint32_t timestamp;
  if (factory_data_timestamp_get(factory_data, &timestamp) == 0) {
    factory_file_write_u32("timestamp", timestamp);
  }

  uint8_t nap_key[16];
  if (factory_data_nap_key_get(factory_data, nap_key) == 0) {
    factory_file_write_hex_string("nap_key", nap_key, sizeof(nap_key));
  }

  uint8_t mac_address[6];
  if (factory_data_mac_address_get(factory_data, mac_address) == 0) {
    factory_file_write_hex_string("mac_address",
                                   mac_address, sizeof(mac_address));
  }

  exit(EXIT_SUCCESS);
}
