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
#include <libsbp/settings.h>

#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "sbp_zmq.h"

#include "minIni/minIni.h"

#define SETTINGS_FILE "/persistent/config.ini"
#define BUFSIZE 256

#define log_error(...) fprintf(stderr, __VA_ARGS__)

struct setting {
  char section[BUFSIZE];
  char name[BUFSIZE];
  char type[BUFSIZE];
  char value[BUFSIZE];
  struct setting *next;
  bool dirty;
};

static struct setting *settings_head;

/* Register a new setting in our linked list */
static void settings_register(struct setting *setting)
{
  struct setting *s;

  if (!settings_head) {
    settings_head = setting;
  } else {
    for (s = settings_head; s->next; s = s->next) {
      if ((strcmp(s->section, setting->section) == 0) &&
          (strcmp(s->next->section, setting->section) != 0))
        break;
    }
    setting->next = s->next;
    s->next = setting;
  }
  char buf[BUFSIZE];
  ini_gets(setting->section, setting->name, "", buf, sizeof(buf), SETTINGS_FILE);
  if (buf[0] != 0) {
    /* Use value from config file */
    strncpy(setting->value, buf, BUFSIZE);
    setting->dirty = true;
  }
}

/* Lookup setting in our linked list */
static struct setting *settings_lookup(const char *section, const char *setting)
{
  for (struct setting *s = settings_head; s; s = s->next)
    if ((strcmp(s->section, section)  == 0) &&
        (strcmp(s->name, setting) == 0))
      return s;
  return NULL;
}

/* Format setting into SBP message payload */
static int settings_format_setting(struct setting *s, char *buf, int len, bool type)
{
  int buflen;

  /* build and send reply */
  strncpy(buf, s->section, len);
  buflen = strlen(s->section) + 1;
  strncpy(buf + buflen, s->name, len - buflen);
  buflen += strlen(s->name) + 1;
  strncpy(buf + buflen, s->value, len - buflen);
  buflen += strlen(s->value) + 1;
  if (type && s->type[0]) {
    strncpy(buf + buflen, s->type, len - buflen);
    buflen += strlen(s->type) + 1;
    buf[buflen++] = '\0';
  }

  return buflen;
}

/* Parse SBP message payload into setting parameters */
static bool settings_parse_setting(u8 len, u8 msg[],
                                   const char **section,
                                   const char **setting,
                                   const char **value,
                                   const char **type)
{
  *section = NULL;
  *setting = NULL;
  *value = NULL;
  if (type)
    *type = NULL;

  if (len == 0) {
    return false;
  }

  if (msg[len-1] != '\0') {
    return false;
  }

  /* Extract parameters from message:
   * 3 null terminated strings: section, setting and value
   * An optional fourth string is a description of the type.
   */
  *section = (const char *)msg;
  for (int i = 0, tok = 0; i < len; i++) {
    if (msg[i] == '\0') {
      tok++;
      switch (tok) {
      case 1:
        *setting = (const char *)&msg[i+1];
        break;
      case 2:
        if (i + 1 < len)
          *value = (const char *)&msg[i+1];
        break;
      case 3:
        if (i + 1 < len) {
          if (type != NULL)
            *type = (const char *)&msg[i+1];
          break;
	}
      case 4:
        if (i == len-1)
          break;
      default:
        return false;
      }
    }
  }

  return true;
}

static void settings_register_callback(u16 sender_id, u8 len, u8 msg[], void* context)
{
  (void)sender_id;
  const char *section = NULL, *setting = NULL, *value = NULL, *type = NULL;
  if (!settings_parse_setting(len, msg, &section, &setting, &value, &type))
    log_error("Error in register message");

  struct setting *s = calloc(1, sizeof(*s));
  strncpy(s->section, section, BUFSIZE);
  strncpy(s->name, setting, BUFSIZE);
  strncpy(s->value, value, BUFSIZE);
  if (type != NULL)
    strncpy(s->type, type, BUFSIZE);

  settings_register(s);

  /* Reply with write message with our value */
  char buf[256];
  size_t rlen = settings_format_setting(s, buf, sizeof(buf), false);
  sbp_zmq_send_msg(context, SBP_MSG_SETTINGS_WRITE, rlen, (u8*)buf);
}

static void settings_read_reply_callback(u16 sender_id, u8 len, u8 msg[], void* context)
{
  (void) context;

  static struct setting *s = NULL;
  const char *section = NULL, *setting = NULL, *value = NULL;

  if(!settings_parse_setting(len, msg, &section, &setting, &value, NULL))
    log_error("Error in register message");

  s = settings_lookup(section, setting);
  if (s == NULL) {
    log_error("Read reply for non-existant setting");
    return;
  }

  if (strcmp(s->value, value) == 0) {
    /* Setting unchanged */
    return;
  }

  /* This is an assignment, call notify function */
  strncpy(s->value, value, BUFSIZE);
  s->dirty = true;

  return;
}

static void settings_read_by_index_callback(u16 sender_id, u8 len, u8 msg[], void* context)
{
  if (sender_id != SBP_SENDER_ID) {
    log_error("Invalid sender");
    return;
  }

  struct setting *s = settings_head;
  char buf[256];
  u8 buflen = 0;

  if (len != 2) {
    log_error("Invalid length for settings read by index!");
    return;
  }
  u16 index = (msg[1] << 8) | msg[0];

  for (int i = 0; (i < index) && s; i++, s = s->next)
    ;

  if (s == NULL) {
    sbp_zmq_send_msg(context, SBP_MSG_SETTINGS_READ_BY_INDEX_DONE, 0, NULL);
    return;
  }

  /* build and send reply */
  buf[buflen++] = msg[0];
  buf[buflen++] = msg[1];
  buflen += settings_format_setting(s, buf + buflen, sizeof(buf) - buflen, true);
  sbp_zmq_send_msg(context, SBP_MSG_SETTINGS_READ_BY_INDEX_RESP, buflen, (void*)buf);
}

static void settings_save_callback(u16 sender_id, u8 len, u8 msg[], void* context)
{
  FILE *f = fopen(SETTINGS_FILE, "w");
  const char *sec;

  (void)sender_id; (void) context; (void)len; (void)msg;

  if (f == NULL) {
    perror("Error opening config file!");
    return;
  }

  for (struct setting *s = settings_head; s; s = s->next) {
    /* Skip unchanged parameters */
    if (!s->dirty)
      continue;

    if ((sec == NULL) || (strcmp(s->section, sec) != 0)) {
      /* New section, write section header */
      sec = s->section;
      fprintf(f, "[%s]\n", sec);
    }

    /* Write setting */
    fprintf(f, "%s=%s\n", s->name, s->value);
  }

  fclose(f);
}

void settings_setup(sbp_state_t *sbp)
{
  sbp_zmq_register_callback(sbp, SBP_MSG_SETTINGS_SAVE,
                            settings_save_callback);
  sbp_zmq_register_callback(sbp, SBP_MSG_SETTINGS_READ_RESP,
                            settings_read_reply_callback);
  sbp_zmq_register_callback(sbp, SBP_MSG_SETTINGS_READ_BY_INDEX_REQ,
                            settings_read_by_index_callback);
  sbp_zmq_register_callback(sbp, SBP_MSG_SETTINGS_REGISTER,
                            settings_register_callback);
}

