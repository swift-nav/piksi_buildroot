/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libsbp/sbp.h>
#include <libsbp/logging.h>
#include <czmq.h>
#include <getopt.h>
#include <unistd.h>

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255
static u16 sbp_sender_id = SBP_SENDER_ID;
static zsock_t *zpub;
static sbp_state_t sbp;
static u8 buf[SBP_FRAMING_MAX_PAYLOAD_SIZE];
static u8 buf_len;

static u32 sbp_write(u8 *b, u32 n, void *context)
{
  (void)context;
  n = MIN(n, sizeof(buf));
  memcpy(&buf[buf_len], b, n);
  buf_len += n;
  return n;
}

static void sbp_write_flush(void)
{
  zmsg_t *msg = zmsg_new();
  zmsg_addmem(msg, buf, buf_len);
  zmsg_send(&msg, zpub);
  buf_len = 0;
}

static void sbp_send_msg(sbp_state_t *sbp, u16 msg_type, u8 len, u8 buff[])
{
  sbp_send_message(sbp, msg_type, sbp_sender_id, len, buff, sbp_write);
  sbp_write_flush();
}

static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    fprintf(stderr, "error opening %s\n", filename);
    return -1;
  }

  bool success = (fgets(str, str_size, fp) != NULL);

  fclose(fp);

  if (!success) {
    fprintf(stderr, "error reading %s\n", filename);
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  char sbp_sender_id_string[32];
  if (file_read_string("/cfg/sbp_sender_id", sbp_sender_id_string,
                        sizeof(sbp_sender_id_string)) == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }

  msg_log_t *msg = alloca(SBP_FRAMING_MAX_PAYLOAD_SIZE);
  const static struct option long_options[] = {
    {"emerg", no_argument, NULL, 0},
    {"alert", no_argument, NULL, 1},
    {"crit", no_argument, NULL, 2},
    {"error", no_argument, NULL, 3},
    {"warn", no_argument, NULL, 4},
    {"notice", no_argument, NULL, 5},
    {"info", no_argument, NULL, 6},
    {"debug", no_argument, NULL, 7},
    {0, 0, 0, 0},
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, NULL)) > 0) {
    if ((unsigned)opt > 7) {
      fprintf(stderr, "Invalid argument\n");
      return -1;
    }
    msg->level = opt;
  }

  sbp_state_init(&sbp);
  zpub =  zsock_new_pub(">tcp://localhost:43011");
  /* Delay for long enough for socket thread to sort itself out */
  usleep(100000);

  while (fgets(msg->text,
               SBP_FRAMING_MAX_PAYLOAD_SIZE - offsetof(msg_log_t, text),
               stdin)) {
    sbp_send_msg(&sbp, SBP_MSG_LOG, sizeof(*msg) + strlen(msg->text), (u8*)msg);
  }

  zsock_destroy(&zpub);

  return 0;
}
