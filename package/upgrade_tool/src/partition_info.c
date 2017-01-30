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

#include "partition_info.h"

#define ERASE_SIZE_MAX 0x00040000U

static int mtd_sysfs_read_int(uint32_t mtd_num, const char *property,
                              uint32_t *value)
{
  /* get sysfs path */
  char path[256];
  snprintf(path, sizeof(path), "/sys/class/mtd/mtd%d/%s", mtd_num, property);

  /* open file */
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    printf("error opening %s\n", path);
    return -1;
  }

  /* read sysfs property */
  char rbuffer[64];
  ssize_t n = read(fd, rbuffer, sizeof(rbuffer));
  if (n <= 0) {
    printf("error reading %s\n", path);
    return -1;
  }
  rbuffer[n-1] = 0;

  /* parse value */
  uint32_t value_parsed = strtol(rbuffer, NULL, 0);

  /* print value and compare with read string */
  char wbuffer[64];
  snprintf(wbuffer, sizeof(wbuffer), "%d", value_parsed);
  if (strcmp(wbuffer, rbuffer) != 0) {
    printf("error parsing %s\n", path);
    return -1;
  }

  /* close file */
  close(fd);

  *value = value_parsed;
  return 0;
}

static int partition_info_populate(partition_info_t *p, uint32_t mtd_num)
{
  if (mtd_sysfs_read_int(mtd_num, "offset", &p->offset) != 0) {
    return -1;
  }

  if (mtd_sysfs_read_int(mtd_num, "size", &p->size) != 0) {
    return -1;
  }

  if (mtd_sysfs_read_int(mtd_num, "erasesize", &p->erasesize) != 0) {
    return -1;
  }

  if (mtd_sysfs_read_int(mtd_num, "numeraseregions",
                         &p->numeraseregions) != 0) {
    return -1;
  }

  p->mtd_num = mtd_num;
  p->valid = true;
  return 0;
}

int partition_info_table_populate(partition_info_t *info_table,
                                  const partition_config_t *config_table,
                                  uint32_t count)
{
  /* clear partition info table */
  memset(info_table, 0, count * sizeof(partition_info_t));

  /* parse lines of /proc/mtd */
  FILE *fp = fopen("/proc/mtd", "r");
  if (fp == NULL) {
    printf("error opening /proc/mtd\n");
    return -1;
  }

  char line[256];

  /* skip header line */
  fgets(line, sizeof(line), fp);

  while (fgets(line, sizeof(line), fp) != NULL) {

    /* expected format: mtd<num> <size> <erasesize> "<name>" */
    int mtd_num;
    unsigned int mtd_size;
    unsigned int mtd_erasesize;
    char mtd_name[128];
    if (sscanf(line, "mtd%d: %x %x \"%127[^\"]\"",
               &mtd_num, &mtd_size, &mtd_erasesize, &mtd_name) != 4) {
      printf("error parsing /proc/mtd line\n");
      continue;
    }

    /* find partition in config and populate info */
    int i;
    for (i=0; i<count; i++) {
      const partition_config_t *c = &config_table[i];

      if (strcmp(c->name, mtd_name) != 0) {
        continue;
      }

      if (partition_info_populate(&info_table[i], mtd_num) != 0) {
        return -1;
      }

      break;
    }
  }

  fclose(fp);
  return 0;
}

int partition_info_table_verify(const partition_info_t *info_table,
                                const partition_config_t *config_table,
                                uint32_t count)
{
  /* make sure all partitions were found and verify info */
  int i;
  for (i=0; i<count; i++) {
    const partition_config_t *c = &config_table[i];
    const partition_info_t *p = &info_table[i];
    if (!p->valid) {
      printf("error: partition %s not found\n", c->name);
      return -1;
    }

    if ((c->required_size != 0) && (c->required_size != p->size)) {
      printf("error: partition %s has invalid size\n", c->name);
      return -1;
    }

    if (p->numeraseregions != 0) {
      printf("error: partition %s has variable erase regions\n", c->name);
      return -1;
    }

    if (p->erasesize > ERASE_SIZE_MAX) {
      printf("error: partition %s erase size too large\n", c->name);
      return -1;
    }
  }

  return 0;
}
