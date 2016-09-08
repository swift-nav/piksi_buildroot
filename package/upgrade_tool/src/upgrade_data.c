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

#include "upgrade_data.h"
#include "uboot/image_table.h"

int upgrade_data_load(const char *filename, const void **upgrade_data,
                      uint32_t *upgrade_data_length)
{
  /* open file */
  int fd = open(filename, O_RDONLY);
  if (fd < 0) {
    printf("error opening %s\n", filename);
    return -1;
  }

  /* stat file */
  struct stat sbuf;
  if (fstat(fd, &sbuf) < 0) {
    printf("error getting status for %s\n", filename);
    return -1;
  }
  uint32_t filesize = sbuf.st_size;

  /* allocate buffer to hold file data */
  void *buffer = malloc(filesize);
  if (buffer == NULL) {
    printf("error allocating buffer for upgrade file\n");
    return -1;
  }

  /* read file into buffer */
  if (read(fd, buffer, filesize) != filesize) {
    printf("error loading upgrade file\n");
    return -1;
  }

  /* close file */
  close(fd);

  *upgrade_data = buffer;
  *upgrade_data_length = filesize;
  return 0;
}

int upgrade_data_release(const void *upgrade_data)
{
  /* free buffer */
  free((void *)upgrade_data);
  return 0;
}

int upgrade_data_verify(const void *upgrade_data, uint32_t upgrade_data_length)
{
  /* make sure there is room for the image set header */
  if (upgrade_data_length < sizeof(image_set_t)) {
    printf("error: upgrade file is too small\n");
    return -1;
  }

  /* verify image set header */
  const image_set_t *image_set = (const image_set_t *)upgrade_data;
  if (image_set_verify(image_set) != 0) {
    printf("error: upgrade file header verification failed\n");
    return -1;
  }

  /* verify image data is present */
  if (upgrade_data_length <= sizeof(image_set_t)) {
    printf("error: upgrade file contains no image data\n");
    return -1;
  }

  /* verify image data */
  int i;
  for (i=0; i<IMAGE_SET_DESCRIPTORS_COUNT; i++) {
    const image_descriptor_t *d = &image_set->descriptors[i];
    if (image_descriptor_type_get(d) != IMAGE_TYPE_INVALID) {

      uint32_t data_offset = image_descriptor_data_offset_get(d);
      uint32_t data_size = image_descriptor_data_size_get(d);

      /* make sure there is room for the image data */
      if ((data_offset >= upgrade_data_length) ||
          (data_size > upgrade_data_length - data_offset)) {
        printf("error: upgrade file data verification failed\n");
        return -1;
      }

      /* verify data CRC */
      const uint8_t *data = &((const uint8_t *)upgrade_data)[data_offset];
      uint32_t computed_data_crc;
      image_descriptor_data_crc_init(&computed_data_crc);
      image_descriptor_data_crc_continue(&computed_data_crc, data, data_size);

      if (image_descriptor_data_crc_get(d) != computed_data_crc) {
        printf("error: upgrade file data verification failed\n");
        return -1;
      }
    }
  }

  return 0;
}
