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

#include "factory_params.h"
#include "uboot/factory_data.h"

int factory_params_read(uint32_t *hardware)
{
  /* open file */
  int fd = open("/factory/mtd", O_RDONLY);
  if (fd < 0) {
    printf("error opening /factory/mtd\n");
    return -1;
  }

  /* allocate buffer to hold header */
  factory_data_t *factory_data =
      (factory_data_t *)malloc(sizeof(factory_data_t));
  if (factory_data == NULL) {
    printf("error allocating buffer for factory data header\n");
    return -1;
  }

  /* read header */
  if (read(fd, factory_data, sizeof(factory_data_t)) !=
          sizeof(factory_data_t)) {
    printf("error reading /factory/mtd\n");
    return -1;
  }

  /* verify header */
  if (factory_data_header_verify(factory_data) != 0) {
    printf("error verifying factory data header\n");
    return -1;
  }

  /* reallocate buffer to hold header + body */
  uint32_t factory_data_body_size = factory_data_body_size_get(factory_data);
  uint32_t factory_data_size = sizeof(factory_data_t) +
                               factory_data_body_size;
  factory_data = (factory_data_t *)realloc((void *)factory_data,
                                           factory_data_size);
  if (factory_data == NULL) {
    printf("error allocating buffer for factory data\n");
    return -1;
  }

  /* read body */
  if (read(fd, &factory_data->body[0], factory_data_body_size) !=
          factory_data_body_size) {
    printf("error reading /factory/mtd\n");
    return -1;
  }

  /* verify body */
  if (factory_data_body_verify(factory_data) != 0) {
    printf("error verifying factory data body\n");
    return -1;
  }

  /* read params */
  if (factory_data_hardware_get(factory_data, hardware) != 0) {
    printf("error reading hardware parameter from factory data\n");
    return -1;
  }

  /* close file */
  close(fd);

  /* free buffer */
  free((void *)factory_data);

  return 0;
}
