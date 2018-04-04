/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/settings.h>


#include "firmware_state.h"
#include "manage_led.h"

#define PROGRAM_NAME "piksi_leds"

#define PUB_ENDPOINT_EXTERNAL_SBP ">tcp://localhost:43031"
#define SUB_ENDPOINT_EXTERNAL_SBP ">tcp://localhost:43030"

#define DURO_EEPROM_PATH "/sys/devices/soc0/amba/e0005000.i2c/i2c-1/1-0050/eeprom"
#define DURO_EEPROM_TEMPFS_PATH "/config/host_board_eeprom"

/* eeprom_id has 6 chars but leave room for null terminator */
static char eeprom_id[7] = "UNK_ID"; 

static bool read_eeprom(void)
{
  int fd = open(DURO_EEPROM_PATH, O_RDONLY);
  if (fd < 0)
    return false;
  read(fd, eeprom_id, 6); 
  close(fd);
}
static bool write_eeprom(void)
{
  int fd = open(DURO_EEPROM_TEMPFS_PATH, O_WRONLY);
  if (fd < 0)
    return false;
  write(fd, eeprom_id, 7); 
  close(fd);
}

static bool board_is_duro(void)
{
  return memcmp(eeprom_id, "DUROV0", 6) == 0;
}

int main(void)
{
  logging_init(PROGRAM_NAME);

  settings_ctx_t *settings_ctx = settings_create();
  if (settings_ctx == NULL) {
    exit(EXIT_FAILURE);
  }
  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  sbp_zmq_pubsub_ctx_t *ctx = sbp_zmq_pubsub_create(PUB_ENDPOINT_EXTERNAL_SBP,
                                                    SUB_ENDPOINT_EXTERNAL_SBP);
  if (ctx == NULL) {
    exit(EXIT_FAILURE);
  }

  firmware_state_init(sbp_zmq_pubsub_rx_ctx_get(ctx));
  read_eeprom();
  write_eeprom();
  
  settings_register_readonly(settings_ctx, "system_info", "host_board_eeprom",
                             eeprom_id, sizeof(eeprom_id),
                             SETTINGS_TYPE_STRING);
  
  manage_led_setup(board_is_duro());

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(ctx));

  sbp_zmq_pubsub_destroy(&ctx);
  exit(EXIT_SUCCESS);
}
