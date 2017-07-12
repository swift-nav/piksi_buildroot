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

#include "uboot/image_table.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define IMAGE_TABLE_ELEMENT_SIZE 0x00040000U
#define IMAGE_TABLE_ELEMENT_COUNT 4
#define IMAGE_TABLE_SIZE (IMAGE_TABLE_ELEMENT_COUNT * IMAGE_TABLE_ELEMENT_SIZE)

#define LOADER_SIZE 0x00040000U
#define ZYNQ_IMAGE_USER_TIME_OFFSET 0x2CU
#define ZYNQ_IMAGE_USER_TIME_SIZE 4
#define ZYNQ_IMAGE_USER_NAME_OFFSET 0x8A0U
#define ZYNQ_IMAGE_USER_NAME_SIZE 32

#define REG_REBOOT_STATUS 0xF8000258U

static int reboot_status_read(uint32_t *reboot_status)
{
  /* open /dev/mem */
  int fd = open("/dev/mem", O_RDONLY);
  if (fd < 0) {
    printf("error opening /dev/mem\n");
    return -1;
  }

  /* align to page size */
  uint32_t page_size = sysconf(_SC_PAGESIZE);
  uint32_t page_base = REG_REBOOT_STATUS & ~(page_size - 1);
  uint32_t page_offset = REG_REBOOT_STATUS - page_base;

  /* mmap /dev/mem */
  const void *ptr = mmap(0, page_size, PROT_READ, MAP_SHARED, fd, page_base);
  if (ptr == MAP_FAILED) {
    printf("error mapping /dev/mem\n");
    return -1;
  }

  /* read reboot status register */
  *reboot_status = *(const uint32_t *)((const uint8_t *)ptr + page_offset);

  /* munmap and close /dev/mem */
  munmap((void *)ptr, page_size);
  close(fd);
  return 0;
}

static int partition_data_load(const char *filename, uint32_t data_length,
                               const void **data)
{
  /* open file */
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    printf("error opening %s\n", filename);
    return -1;
  }

  /* allocate buffer */
  void *buffer = malloc(data_length);
  if (buffer == NULL) {
    printf("error allocating buffer for partition data\n");
    return -1;
  }

  /* read data */
  if (read(fd, buffer, data_length) != data_length) {
    printf("error reading %s\n", filename);
    return -1;
  }

  /* close file */
  close(fd);

  *data = buffer;
  return 0;
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

static int img_tbl_file_write(const char *filepath, const char *filename,
                              const char *data)
{
  /* generate file path */
  char path[256];
  snprintf(path, sizeof(path), "/img_tbl/%s/%s", filepath, filename);

  /* open file */
  int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0) {
    printf("error opening %s\n", path);
    return -1;
  }

  /* write to file */
  size_t data_len = strlen(data);
  if (write(fd, data, data_len) != data_len) {
    printf("error writing %s\n", path);
  }

  /* close file */
  close(fd);

  return 0;
}

static int img_tbl_file_write_u32(const char *filepath, const char *filename,
                                  uint32_t data)
{
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%d", data);
  return img_tbl_file_write(filepath, filename, buffer);
}

static int img_tbl_file_write_hex_string(const char *filepath,
                                         const char *filename,
                                         const uint8_t *data,
                                         uint32_t data_size)
{
  char buffer[2 * data_size + 1];
  print_hex_string(buffer, data, data_size);
  return img_tbl_file_write(filepath, filename, buffer);
}

static int img_set_output(const char *filepath, const image_set_t *image_set)
{
  /* name */
  uint8_t name[32 + 1];
  image_set_name_get(image_set, name);
  name[sizeof(name) - 1] = 0;
  img_tbl_file_write(filepath, "name", name);

  /* timestamp */
  uint32_t timestamp = image_set_timestamp_get(image_set);
  img_tbl_file_write_u32(filepath, "timestamp", timestamp);
}

int main(int argc, char *argv[])
{
  /* get image table data */
  const void *image_table;
  if (partition_data_load("/img_tbl/mtd", IMAGE_TABLE_SIZE,
                          &image_table) != 0) {
    exit(EXIT_FAILURE);
  }

  /* get loader data */
  const void *loader;
  if (partition_data_load("/img_tbl/loader/mtd", LOADER_SIZE,
                          &loader) != 0) {
    exit(EXIT_FAILURE);
  }

  /* read reboot status register */
  uint32_t reboot_status;
  if (reboot_status_read(&reboot_status) != 0) {
    exit(EXIT_FAILURE);
  }

  /* image table index is stored in the lower two bits
   * of REBOOT_STATUS by the loader */
  uint32_t boot_image_table_index = reboot_status & 0x3;

  const image_set_t *image_set_boot =
      (const image_set_t *)
      &((const uint8_t *)image_table)[boot_image_table_index *
                                      IMAGE_TABLE_ELEMENT_SIZE];

  if (image_set_verify(image_set_boot) == 0) {
    mkdir("/img_tbl/boot", 0777);
    img_set_output("boot", image_set_boot);
  } else {
    printf("warning: boot image set verification failed\n");
  }

  /* loader version */
  char loader_name[ZYNQ_IMAGE_USER_NAME_SIZE + 1];
  memcpy(loader_name, &((const uint8_t *)loader)[ZYNQ_IMAGE_USER_NAME_OFFSET],
         ZYNQ_IMAGE_USER_NAME_SIZE);
  loader_name[ZYNQ_IMAGE_USER_NAME_SIZE] = 0;
  img_tbl_file_write("loader", "name", loader_name);

  /* loader timestamp */
  uint32_t loader_timestamp =
      le32_to_cpu(*(uint32_t *)
                  &((const uint8_t *)loader)[ZYNQ_IMAGE_USER_TIME_OFFSET]);
  img_tbl_file_write_u32("loader", "timestamp", loader_timestamp);

  exit(EXIT_SUCCESS);
}
