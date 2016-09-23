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

#include "singleton.h"
#include "partition_info.h"
#include "upgrade_data.h"
#include "mtd.h"
#include "factory_params.h"
#include "uboot/image_table.h"

#define IMAGE_TABLE_ELEMENT_SIZE  0x00010000U
#define SPL_TABLE_ELEMENT_SIZE    0x00040000U

#define IMAGE_ALIGN 16U

#define IMAGE_MAP_COUNT IMAGE_SET_DESCRIPTORS_COUNT

#define REG_REBOOT_STATUS 0xF8000258U

typedef enum {
  TABLE_INDEX_FAILSAFE_A,
  TABLE_INDEX_FAILSAFE_B,
  TABLE_INDEX_STANDARD_A,
  TABLE_INDEX_STANDARD_B
 } table_index_t;

typedef enum {
  PARTITION_INDEX_IMAGE_TABLE,
  PARTITION_INDEX_SPL_TABLE,
  PARTITION_INDEX_IMAGE_STANDARD_A,
  PARTITION_INDEX_IMAGE_STANDARD_B,
  PARTITION_INDEX__COUNT
} partition_index_t;

typedef struct {
  bool valid;
  uint32_t source_offset;
  uint32_t size;
  partition_index_t destination_partition_index;
  uint32_t destination_partition_offset;
  uint32_t descriptor_index;
} image_map_t;

static const partition_config_t partition_config_table[] = {
  [PARTITION_INDEX_IMAGE_TABLE] =
    { .name = "qspi-image-table", .required_size = 4*IMAGE_TABLE_ELEMENT_SIZE },
  [PARTITION_INDEX_SPL_TABLE] =
    { .name = "qspi-spl-table",   .required_size = 4*SPL_TABLE_ELEMENT_SIZE },
  [PARTITION_INDEX_IMAGE_STANDARD_A] =
    { .name = "qspi-img-std-a",   .required_size = 0 },
  [PARTITION_INDEX_IMAGE_STANDARD_B] =
    { .name = "qspi-img-std-b",   .required_size = 0 },
};

static partition_info_t partition_info_table[PARTITION_INDEX__COUNT];
static image_map_t image_map_table[IMAGE_MAP_COUNT];
static image_set_t target_image_set;

static bool debug = false;
static const char *upgrade_filename = NULL;

static void usage(char *command)
{
  printf("Usage: %s [<options>] <upgrade_file>\n", command);

  puts("\nMisc options");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_DEBUG = 1
  };

  const struct option long_opts[] = {
    {"debug",       no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0}
  };

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "",
                          long_opts, &opt_index)) != -1) {
    switch (c) {
      case OPT_ID_DEBUG: {
        debug = true;
      }
      break;

      default: {
        printf("invalid option\n");
        return -1;
      }
      break;
    }
  }

  if (optind != (argc-1)) {
    printf("upgrade file not specified\n");
    return -1;
  }

  upgrade_filename = argv[optind++];
  return 0;
}

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

static int image_set_mtd_read(uint32_t image_table_index,
                              image_set_t *image_set)
{
  const partition_info_t *p_image_table =
      &partition_info_table[PARTITION_INDEX_IMAGE_TABLE];

  uint32_t offset = image_table_index * IMAGE_TABLE_ELEMENT_SIZE;

  /* read image set header from flash */
  if (mtd_read(p_image_table, offset, image_set, sizeof(image_set_t)) != 0) {
    return -1;
  }

  return 0;
}

static int image_data_mtd_crc_compute(uint32_t data_offset, uint32_t data_size,
                                      uint32_t *data_crc)
{
  /* find partition which contains this offset */
  int i;
  for (i=0; i<PARTITION_INDEX__COUNT; i++) {
    const partition_info_t *p = &partition_info_table[i];

    if ((p->offset <= data_offset) &&
        (data_offset < (p->offset + p->size))) {

      uint32_t partition_offset = data_offset - p->offset;
      if ((partition_offset + data_size) > p->size) {
        printf("error: target image exceeds partition size\n");
        return -1;
      }

      /* compute CRC from flash */
      return mtd_crc_compute(p, partition_offset, data_size, data_crc);
    }
  }

  printf("error: could not find target image partition\n");
  return -1;
}

static int image_set_ram_verify_mtd(const image_set_t *image_set)
{
  /* verify image set header */
  if (image_set_verify(image_set) != 0) {
    return -1;
  }

  /* verify image data in flash */
  int i;
  for (i=0; i<IMAGE_SET_DESCRIPTORS_COUNT; i++) {
    const image_descriptor_t *d = &image_set->descriptors[i];
    if (image_descriptor_type_get(d) != IMAGE_TYPE_INVALID) {

      uint32_t computed_data_crc;
      if (image_data_mtd_crc_compute(image_descriptor_data_offset_get(d),
                                     image_descriptor_data_size_get(d),
                                     &computed_data_crc) != 0) {
        return -1;
      }

      if (image_descriptor_data_crc_get(d) != computed_data_crc) {
        return -1;
      }
    }
  }

  return 0;
}

static int image_set_mtd_verify_mtd(uint32_t image_table_index)
{
  /* read image set header from flash */
  image_set_t image_set;
  if (image_set_mtd_read(image_table_index, &image_set) != 0) {
    return -1;
  }

  /* verify image set header in RAM and image data in flash */
  return image_set_ram_verify_mtd(&image_set);
}

static int image_set_mtd_seq_num_get(uint32_t image_table_index,
                                     uint32_t *seq_num)
{
  /* read image set header from flash */
  image_set_t image_set;
  if (image_set_mtd_read(image_table_index, &image_set) != 0) {
    return -1;
  }

  /* verify image set header in RAM and image data in flash */
  if (image_set_ram_verify_mtd(&image_set) != 0) {
    return -1;
  }

  /* get seq num */
  *seq_num = image_set_seq_num_get(&image_set);
  return 0;
}

static int target_params_get(uint32_t *image_table_index,
                             partition_index_t *std_partition_index,
                             uint32_t *seq_num)
{
  /* read reboot status register */
  uint32_t reboot_status;
  if (reboot_status_read(&reboot_status) != 0) {
    return -1;
  }

  /* image table index is stored in the lower two bits
   * of REBOOT_STATUS by the loader */
  uint32_t boot_image_table_index = reboot_status & 0x3;
  bool failsafe_boot = ((boot_image_table_index == TABLE_INDEX_FAILSAFE_A) ||
                        (boot_image_table_index == TABLE_INDEX_FAILSAFE_B));

  /* determine target table index and seq num */
  uint32_t target_image_table_index;
  uint32_t target_seq_num;
  if (failsafe_boot) {
    /* failsafe boot: write to the oldest valid standard region */

    /* determine validity and get seq nums */
    uint32_t seq_num_a = 0;
    bool image_set_a_valid =
        (image_set_mtd_seq_num_get(TABLE_INDEX_STANDARD_A, &seq_num_a) == 0);
    uint32_t seq_num_b = 0;
    bool image_set_b_valid =
        (image_set_mtd_seq_num_get(TABLE_INDEX_STANDARD_B, &seq_num_b) == 0);

    if (image_set_a_valid && image_set_b_valid) {
      /* both valid, install to oldest determined by seq num */
      int32_t seq_num_diff = seq_num_a - seq_num_b;
      target_image_table_index =
          (seq_num_diff < 0) ? TABLE_INDEX_STANDARD_A : TABLE_INDEX_STANDARD_B;
    } else if (image_set_a_valid) {
      /* only A valid, install to B */
      target_image_table_index = TABLE_INDEX_STANDARD_B;
    } else if (image_set_b_valid) {
      /* only B valid, install to A */
      target_image_table_index = TABLE_INDEX_STANDARD_A;
    } else {
      /* no valid image set, install to A */
      target_image_table_index = TABLE_INDEX_STANDARD_A;
    }

    target_seq_num = ((target_image_table_index == TABLE_INDEX_STANDARD_A) ?
                      seq_num_a : seq_num_b) + 1;

  } else {
    /* standard boot: write to the region not used for the current boot */

    /* make sure booted image set is valid and get seq num */
    uint32_t boot_seq_num = 0;
    if (image_set_mtd_seq_num_get(boot_image_table_index,
                                  &boot_seq_num) != 0) {
      printf("error: detected boot image set is not valid\n");
      return -1;
    }

    target_image_table_index =
        (boot_image_table_index == TABLE_INDEX_STANDARD_A) ?
        TABLE_INDEX_STANDARD_B : TABLE_INDEX_STANDARD_A;

    target_seq_num = boot_seq_num + 1;
  }

  /* select image partition to match table index */
  partition_index_t target_std_partition_index =
      (target_image_table_index == TABLE_INDEX_STANDARD_A) ?
      PARTITION_INDEX_IMAGE_STANDARD_A : PARTITION_INDEX_IMAGE_STANDARD_B;

  debug_printf("installing to region %s\n",
               (target_std_partition_index ==
                    PARTITION_INDEX_IMAGE_STANDARD_A) ? "A" : "B");

  *image_table_index = target_image_table_index;
  *std_partition_index = target_std_partition_index;
  *seq_num = target_seq_num;
  return 0;
}

static int image_map_table_populate(const image_set_t *upgrade_image_set,
                                    uint32_t target_image_table_index,
                                    partition_index_t
                                    target_std_partition_index)
{
  /* clear image map table */
  memset(image_map_table, 0, sizeof(image_map_table));

  /* map each image descriptor */
  uint32_t image_map_index = 0;
  uint32_t std_partition_offset = 0;
  int i;
  for (i=0; i<IMAGE_SET_DESCRIPTORS_COUNT; i++) {
    const image_descriptor_t *d = &upgrade_image_set->descriptors[i];
    uint32_t type = image_descriptor_type_get(d);
    if (type != IMAGE_TYPE_INVALID) {

      if (image_map_index >= PARTITION_INDEX__COUNT) {
        printf("error: image map table exhausted\n");
        return -1;
      }

      image_map_t *m = &image_map_table[image_map_index];
      uint32_t data_offset = image_descriptor_data_offset_get(d);
      uint32_t data_size = image_descriptor_data_size_get(d);

      switch (type) {
        case IMAGE_TYPE_LOADER: {
          printf("error: loader upgrade not supported\n");
          return -1;
        }
        break;

        case IMAGE_TYPE_UBOOT_SPL: {
          /* SPL image is written to a slot in the SPL table partition
           * specified by the image table index */
          m->destination_partition_index = PARTITION_INDEX_SPL_TABLE;
          m->destination_partition_offset =
              target_image_table_index * SPL_TABLE_ELEMENT_SIZE;
        }
        break;

        default: {
          /* all other image types are written sequentially to the specified
           * standard partition */
          m->destination_partition_index = target_std_partition_index;
          m->destination_partition_offset = std_partition_offset;

          std_partition_offset += data_size;
          if ((std_partition_offset % IMAGE_ALIGN) != 0) {
            std_partition_offset += IMAGE_ALIGN -
                                    (std_partition_offset % IMAGE_ALIGN);
          }
        }
        break;
      }

      m->source_offset = data_offset;
      m->size = data_size;
      m->descriptor_index = i;
      m->valid = true;
      image_map_index++;
    }
  }

  return 0;
}

static int image_map_table_verify(uint32_t target_image_table_index,
                                  partition_index_t target_std_partition_index)
{
  const partition_info_t *p_std =
      &partition_info_table[target_std_partition_index];

  bool spl_present = false;

  int i;
  for (i=0; i<PARTITION_INDEX__COUNT; i++) {
    const image_map_t *m = &image_map_table[i];
    if (m->valid) {
      if (m->destination_partition_index == PARTITION_INDEX_SPL_TABLE) {
        /* SPL: must be exactly one image, must fit in table slot */
        if (spl_present) {
          printf("error: multiple SPL images in upgrade file\n");
          return -1;
        }
        spl_present = true;

        if (m->destination_partition_offset !=
                (target_image_table_index * SPL_TABLE_ELEMENT_SIZE)) {
          printf("error: invalid SPL offset\n");
          return -1;
        }

        if (m->size > SPL_TABLE_ELEMENT_SIZE) {
          printf("error: SPL image size too large\n");
          return -1;
        }
      } else if (m->destination_partition_index ==
                     target_std_partition_index) {
        /* standard images: must be aligned, must fit in partition */
        if ((m->destination_partition_offset % IMAGE_ALIGN) != 0) {
          printf("error: image alignment failed\n");
          return -1;
        }

        if (m->destination_partition_offset + m->size > p_std->size) {
          printf("error: upgrade image size too large\n");
          return -1;
        }
      } else {
        printf("error: invalid destination partition in image map\n");
        return -1;
      }
    }
  }

  if (!spl_present) {
    printf("error: no SPL image in upgrade file\n");
    return -1;
  }

  return 0;
}

static int partition_write(partition_index_t partition_index,
                           const void *upgrade_data)
{
  /* write all images mapped to the specified partition */
  int i;
  for (i=0; i<IMAGE_MAP_COUNT; i++) {
    const image_map_t *m = &image_map_table[i];
    if (m->valid) {
      if (m->destination_partition_index == partition_index) {
        const uint8_t *source_data =
            &((const uint8_t *)upgrade_data)[m->source_offset];
        if (mtd_write_and_verify(&partition_info_table[partition_index],
                                 m->destination_partition_offset,
                                 source_data, m->size) != 0) {
          return -1;
        }
      }
    }
  }

  return 0;
}

static int target_image_set_populate(const image_set_t *upgrade_image_set,
                                     uint32_t seq_num)
{
  /* copy image set from upgrade file */
  memcpy(&target_image_set, upgrade_image_set, sizeof(image_set_t));

  /* update the descriptor associated with each image map */
  uint32_t i;
  for (i=0; i<IMAGE_MAP_COUNT; i++) {
    const image_map_t *m = &image_map_table[i];
    if (m->valid) {
      image_descriptor_t *d =
          &target_image_set.descriptors[m->descriptor_index];

      /* target data offset = partition start offset +
       * image offset within partition */
      uint32_t target_data_offset =
          partition_info_table[m->destination_partition_index].offset +
          m->destination_partition_offset;
      image_descriptor_data_offset_set(d, target_data_offset);
    }
  }

  /* set seq num */
  image_set_seq_num_set(&target_image_set, seq_num);

  /* update CRC */
  image_set_finalize(&target_image_set);

  return 0;
}

static int upgrade_install(const void *upgrade_data,
                           uint32_t target_image_table_index,
                           partition_index_t target_std_partition_index)
{
  /* erase image set header from image table */
  if (mtd_erase(&partition_info_table[PARTITION_INDEX_IMAGE_TABLE],
                target_image_table_index * IMAGE_TABLE_ELEMENT_SIZE,
                IMAGE_TABLE_ELEMENT_SIZE) != 0) {
    return -1;
  }

  /* make sure verification fails after erase */
  if (image_set_mtd_verify_mtd(target_image_table_index) == 0) {
    printf("error: error erasing image set header\n");
    return -1;
  }

  /* erase SPL from SPL table */
  if (mtd_erase(&partition_info_table[PARTITION_INDEX_SPL_TABLE],
                target_image_table_index * SPL_TABLE_ELEMENT_SIZE,
                SPL_TABLE_ELEMENT_SIZE) != 0) {
    return -1;
  }

  /* erase standard image partition */
  if (mtd_erase(&partition_info_table[target_std_partition_index],
                0,
                partition_info_table[target_std_partition_index].size) != 0) {
    return -1;
  }

  /* write standard partition images */
  if (partition_write(target_std_partition_index, upgrade_data) != 0) {
    return -1;
  }

  /* write SPL image */
  if (partition_write(PARTITION_INDEX_SPL_TABLE, upgrade_data) != 0) {
    return -1;
  }

  /* verify target image set against written image data */
  if (image_set_ram_verify_mtd(&target_image_set) != 0) {
    printf("error: written image data failed validation\n");
    return -1;
  }

  /* write image set header */
  if (mtd_write_and_verify(&partition_info_table[PARTITION_INDEX_IMAGE_TABLE],
                           target_image_table_index * IMAGE_TABLE_ELEMENT_SIZE,
                           &target_image_set, sizeof(image_set_t)) != 0) {
    return -1;
  }

  /* verify all written data */
  if (image_set_mtd_verify_mtd(target_image_table_index) != 0) {
    printf("error: written image set failed validation\n");
    return -1;
  }

  return 0;
}

void debug_printf(const char *msg, ...)
{
  if (!debug) {
    return;
  }

  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);
}

int main(int argc, char *argv[])
{
  /*  1. read MTD partition info from /proc/mtd
   *  2. verify MTD partition info
   *  3. determine where to install upgrade
   *  4. verify upgrade file integrity
   *  5. verify ability to install upgrade
   *  6. erase image set header from image table
   *  7. erase SPL and target partition
   *  8. write SPL and target partition
   *  9. write image set header to image table
   * 10. final verification of written data
   */

  /* parse options */
  if (parse_options(argc, argv) != 0) {
    usage(basename(argv[0]));
    exit(EXIT_FAILURE);
  }

  /* ensure no other instances of this program are running */
  if (singleton_setup() != 0) {
    printf("error: upgrade already in progress\n");
    exit(EXIT_FAILURE);
  }

  /* get info for MTD partitions */
  if (partition_info_table_populate(partition_info_table,
                                    partition_config_table,
                                    PARTITION_INDEX__COUNT) != 0) {
    exit(EXIT_FAILURE);
  }

  /* verify partition info */
  if (partition_info_table_verify(partition_info_table,
                                  partition_config_table,
                                  PARTITION_INDEX__COUNT) != 0) {
    exit(EXIT_FAILURE);
  }

  /* get target params */
  uint32_t target_image_table_index;
  partition_index_t target_std_partition_index;
  uint32_t target_seq_num;
  if (target_params_get(&target_image_table_index, &target_std_partition_index,
                        &target_seq_num) != 0) {
    exit(EXIT_FAILURE);
  }

  /* load upgrade data to RAM */
  const void *upgrade_data;
  uint32_t upgrade_data_length;
  if (upgrade_data_load(upgrade_filename, &upgrade_data,
                        &upgrade_data_length) != 0) {
    exit(EXIT_FAILURE);
  }

  /* verify upgrade data integrity */
  if (upgrade_data_verify(upgrade_data, upgrade_data_length) != 0) {
    exit(EXIT_FAILURE);
  }

  /* get upgrade image set */
  const image_set_t *upgrade_image_set = (const image_set_t *)upgrade_data;

  /* get factory params */
  uint32_t factory_hardware;
  if (factory_params_read(&factory_hardware) != 0) {
    exit(EXIT_FAILURE);
  }

  /* verify factory params */
  if (factory_hardware != image_set_hardware_get(upgrade_image_set)) {
    printf("error: invalid hardware type\n");
    exit(EXIT_FAILURE);
  }

  /* populate image map table */
  if (image_map_table_populate(upgrade_image_set, target_image_table_index,
                               target_std_partition_index) != 0) {
    exit(EXIT_FAILURE);
  }

  /* verify image map table */
  if (image_map_table_verify(target_image_table_index,
                             target_std_partition_index) != 0) {
    exit(EXIT_FAILURE);
  }

  /* populate target image set */
  if (target_image_set_populate(upgrade_image_set, target_seq_num) != 0) {
    exit(EXIT_FAILURE);
  }

  /* perform upgrade */
  if (upgrade_install(upgrade_data, target_image_table_index,
                      target_std_partition_index) != 0) {
    exit(EXIT_FAILURE);
  }

  /* release upgrade data */
  upgrade_data_release(upgrade_data);

  debug_printf("upgrade completed successfully\n");
  exit(EXIT_SUCCESS);
}
