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

#include "mtd.h"
#include "uboot/image_table.h"

#define READ_CHUNK_LENGTH_MAX 65536
#define WRITE_CHUNK_LENGTH_MAX 65536

typedef enum {
  MTD_OP_ERASE,
  MTD_OP_WRITE,
  MTD_OP_READ
} mtd_op_t;

static int mtd_open(uint32_t mtd_num)
{
  /* get device path */
  char dev[64];
  snprintf(dev, sizeof(dev), "/dev/mtd%d", mtd_num);

  /* open file */
  int fd = open(dev, O_SYNC | O_RDWR);
  if (fd < 0) {
    printf("error opening %s\n", dev);
    return -1;
  }

  return fd;
}

static int mtd_close(int fd)
{
  /* close file */
  close(fd);

  return 0;
}

static int mtd_params_verify(const partition_info_t *partition_info,
                             uint32_t offset, uint32_t length, mtd_op_t mtd_op)
{
  /* verify validity */
  if (!partition_info->valid) {
    printf("error: partition info not valid\n");
    return -1;
  }

  /* verify offset */
  if (offset >= partition_info->size) {
    printf("error: invalid mtd offset: %08x\n", offset);
    return -1;
  }

  /* verify length */
  if (length > partition_info->size - offset) {
    printf("error: invalid mtd length: %08x\n", length);
    return -1;
  }

  if (mtd_op == MTD_OP_ERASE) {
    /* verify offset alignment */
    if (offset % partition_info->erasesize != 0) {
      printf("error: invalid mtd offset alignment: %08x\n", offset);
      return -1;
    }

    /* verify length alignment */
    if (length % partition_info->erasesize != 0) {
      printf("error: invalid mtd length alignment: %08x\n", length);
      return -1;
    }
  }

  return 0;
}

int mtd_erase(const partition_info_t *partition_info, uint32_t offset,
              uint32_t length)
{
  /* verify params */
  if (mtd_params_verify(partition_info, offset, length, MTD_OP_ERASE) != 0) {
    return -1;
  }

  /* open MTD */
  int fd = mtd_open(partition_info->mtd_num);
  if (fd < 0) {
    return -1;
  }

  debug_printf("erasing mtd%d 0x%08x - 0x%08x...\n",
               partition_info->mtd_num, offset, offset + length);

  /* perform erase */
  uint32_t sector_offset;
  for (sector_offset = 0; sector_offset < length;
       sector_offset += partition_info->erasesize) {

    struct erase_info_user erase_info = {
      .start = offset + sector_offset,
      .length = partition_info->erasesize
    };
    if (ioctl(fd, MEMERASE, &erase_info) < 0) {
      printf("error erasing flash\n");
      return -1;
    }

    debug_printf("\r%d %% complete", 100 * sector_offset / length);
    debug_flush();
  }

  debug_printf("\r100 %% complete\n");
  debug_printf("ok\n");

  /* close MTD */
  mtd_close(fd);
  return 0;
}

int mtd_write_and_verify(const partition_info_t *partition_info,
                         uint32_t offset, const void *data, uint32_t length)
{
  /* verify params */
  if (mtd_params_verify(partition_info, offset, length, MTD_OP_WRITE) != 0) {
    return -1;
  }

  /* open MTD */
  int fd = mtd_open(partition_info->mtd_num);
  if (fd < 0) {
    return -1;
  }

  /* seek to offset */
  if (lseek(fd, offset, SEEK_SET) != offset) {
    printf("error seeking flash\n");
    return -1;
  }

  debug_printf("writing mtd%d 0x%08x - 0x%08x...\n",
               partition_info->mtd_num, offset, offset + length);

  /* write data */
  uint32_t write_offset = 0;
  while (write_offset < length) {

    uint32_t write_length = length - write_offset;
    if (write_length > WRITE_CHUNK_LENGTH_MAX) {
      write_length = WRITE_CHUNK_LENGTH_MAX;
    }

    const void *write_data = &((const uint8_t *)data)[write_offset];
    if (write(fd, write_data, write_length) != write_length) {
      printf("error writing flash\n");
      return -1;
    }

    debug_printf("\r%d %% complete", 100 * write_offset / length);
    debug_flush();

    write_offset += write_length;
  }

  /* seek to offset */
  if (lseek(fd, offset, SEEK_SET) != offset) {
    printf("error seeking flash\n");
    return -1;
  }

  /* allocate read buffer */
  void *read_buffer = malloc(READ_CHUNK_LENGTH_MAX);
  if (read_buffer == NULL) {
    printf("error allocating buffer\n");
    return -1;
  }

  /* verify data */
  uint32_t read_offset = 0;
  while (read_offset < length) {

    uint32_t read_length = length - read_offset;
    if (read_length > READ_CHUNK_LENGTH_MAX) {
      read_length = READ_CHUNK_LENGTH_MAX;
    }

    /* read data */
    if (read(fd, read_buffer, read_length) != read_length) {
      printf("error reading flash\n");
      return -1;
    }

    /* compare with source data */
    if (memcmp(read_buffer, &((const uint8_t *)data)[read_offset],
               read_length) != 0) {
      printf("error verifying flash\n");
      return -1;
    }

    read_offset += read_length;
  }

  debug_printf("\r100 %% complete\n");
  debug_printf("ok\n");

  /* free buffer */
  free(read_buffer);

  /* close MTD */
  mtd_close(fd);
  return 0;
}

int mtd_read(const partition_info_t *partition_info,
             uint32_t offset, void *buffer, uint32_t length)
{
  /* verify params */
  if (mtd_params_verify(partition_info, offset, length, MTD_OP_READ) != 0) {
    return -1;
  }

  /* open MTD */
  int fd = mtd_open(partition_info->mtd_num);
  if (fd < 0) {
    return -1;
  }

  /* seek to offset */
  if (lseek(fd, offset, SEEK_SET) != offset) {
    printf("error seeking flash\n");
    return -1;
  }

  uint32_t read_offset = 0;
  while (read_offset < length) {

    uint32_t read_length = length - read_offset;
    if (read_length > READ_CHUNK_LENGTH_MAX) {
      read_length = READ_CHUNK_LENGTH_MAX;
    }

    /* read data */
    void *read_data = &((uint8_t *)buffer)[read_offset];
    if (read(fd, read_data, read_length) != read_length) {
      printf("error reading flash\n");
      return -1;
    }

    read_offset += read_length;
  }

  /* close MTD */
  mtd_close(fd);
  return 0;
}

int mtd_crc_compute(const partition_info_t *partition_info,
                    uint32_t offset, uint32_t length, uint32_t *crc)
{
  /* verify params */
  if (mtd_params_verify(partition_info, offset, length, MTD_OP_READ) != 0) {
    return -1;
  }

  /* open MTD */
  int fd = mtd_open(partition_info->mtd_num);
  if (fd < 0) {
    return -1;
  }

  /* seek to offset */
  if (lseek(fd, offset, SEEK_SET) != offset) {
    printf("error seeking flash\n");
    return -1;
  }

  /* allocate read buffer */
  void *read_buffer = malloc(READ_CHUNK_LENGTH_MAX);
  if (read_buffer == NULL) {
    printf("error allocating buffer\n");
    return -1;
  }

  /* compute CRC */
  image_descriptor_data_crc_init(crc);
  uint32_t read_offset = 0;
  while (read_offset < length) {

    uint32_t read_length = length - read_offset;
    if (read_length > READ_CHUNK_LENGTH_MAX) {
      read_length = READ_CHUNK_LENGTH_MAX;
    }

    /* read data */
    if (read(fd, read_buffer, read_length) != read_length) {
      printf("error reading flash\n");
      return -1;
    }

    /* continue CRC */
    image_descriptor_data_crc_continue(crc, read_buffer, read_length);

    read_offset += read_length;
  }

  /* free buffer */
  free(read_buffer);

  /* close MTD */
  mtd_close(fd);
  return 0;
}
