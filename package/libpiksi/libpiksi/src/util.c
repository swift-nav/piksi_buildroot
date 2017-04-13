/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libpiksi/util.h>
#include <libpiksi/logging.h>
#include <assert.h>

#define SBP_SENDER_ID_FILE_PATH "/cfg/sbp_sender_id"
#define DEVICE_UUID_FILE_PATH   "/cfg/device_uuid"

static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    piksi_log(LOG_ERR, "error opening %s", filename);
    return -1;
  }

  bool success = (fgets(str, str_size, fp) != NULL);

  fclose(fp);

  if (!success) {
    piksi_log(LOG_ERR, "error reading %s", filename);
    return -1;
  }

  return 0;
}

static int zloop_timer_handler(zloop_t *loop, int timer_id, void *arg)
{
  (void)loop;
  (void)timer_id;
  (void)arg;

  return -1;
}

u16 sbp_sender_id_get(void)
{
  u16 sbp_sender_id = SBP_SENDER_ID;

  char sbp_sender_id_string[32];
  if (file_read_string(SBP_SENDER_ID_FILE_PATH, sbp_sender_id_string,
                       sizeof(sbp_sender_id_string)) == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }

  return sbp_sender_id;
}

int device_uuid_get(char *str, size_t str_size)
{
  return file_read_string(DEVICE_UUID_FILE_PATH, str, str_size);
}

int zmq_simple_loop(zloop_t *zloop)
{
  assert(zloop != NULL);

  while (1) {
    int zloop_ret = zloop_start(zloop);
    if (zloop_ret == 0) {
      /* Interrupted */
      continue;
    } else if (zloop_ret == -1) {
      /* Canceled by a handler */
      return 0;
    } else {
      /* Error occurred */
      piksi_log(LOG_ERR, "error in zloop");
      return -1;
    }
  }
}

int zmq_simple_loop_timeout(zloop_t *zloop, u32 timeout_ms)
{
  assert(zloop != NULL);

  zloop_set_ticket_delay(zloop, timeout_ms);

  void *ticket = zloop_ticket(zloop, zloop_timer_handler, NULL);
  if (ticket == NULL) {
    piksi_log(LOG_ERR, "error creating zloop ticket");
    return -1;
  }

  int result = zmq_simple_loop(zloop);
  zloop_ticket_delete(zloop, ticket);
  return result;
}
