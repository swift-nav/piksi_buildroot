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

/* https://swiftnav.hackpad.com/SBP-Message-Whitelist-Design-toM7WBRqq3C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sbp_settings.h>

#include "whitelists.h"

enum port {
  PORT_UART0,
  PORT_UART1,
  PORT_USB0,
  PORT_ETHERNET,
  PORT_MAX
};

const char *section_names[PORT_MAX] = {
  "uart0", "uart1", "usb0", "ethernet",
};

/* Whitelist settings are kept as formatted strings of message ids and
 * rate dividers.  Strings are parsed in whilelist_notify()
 *
 * Examples:
 * ""
 *  - All messages are sent
 * "65535"
 *  - Only the heartbeat message is sent (message ID 65535)
 * "1234,5678/2,3456/10"
 *  - Message 1234 is sent at full rate
 *  - Message 5678 is sent at half rate
 *  - Message 3456 is sent at 1/10 rate
 */
char wl[PORT_MAX][256] ={
  [PORT_UART0] = "68,72,73,74,65535",
  [PORT_UART1] = "",
  [PORT_USB0] = "",
  [PORT_ETHERNET] = "",
};


static bool whitelist_notify(struct setting *s, const char *val)
{
  if (strlen(val) > s->len)
    return false;

  char *c = (char *)val;
  unsigned tmp;
  enum {PARSE_ID, PARSE_AFTER_ID, PARSE_DIV, PARSE_AFTER_DIV} state = PARSE_ID;
  struct {
    unsigned id;
    unsigned div;
  } whitelist[128];
  int entries = 0;

  /* Simple parser for whitelist settings */
  while (*c) {
    switch (*c) {
    /* Integer token, this is an ID or divider */
    case '0' ... '9':
      tmp = strtoul(c, &c, 10);
      switch (state) {
      case PARSE_ID:
        state = PARSE_AFTER_ID;
        whitelist[entries].id = tmp;
        whitelist[entries].div = 1;
        entries++;
        break;
      case PARSE_DIV:
        state = PARSE_AFTER_DIV;
        whitelist[entries-1].div = tmp;
        break;
      default:
        return false;
      }
      break;

    /* Divider token, following is divider */
    case '/':
      if (state == PARSE_AFTER_ID) {
        state = PARSE_DIV;
        c++;
      } else {
        return false;
      }
      break;

    /* Separator token, following is message id */
    case ',':
      if ((state == PARSE_AFTER_ID) || (state == PARSE_AFTER_DIV)) {
        state = PARSE_ID;
        c++;
      } else {
        return false;
      }
      break;

    /* Invalid token, parse error */
    default:
      return false;
    }
  }

  /* Parsed successfully, write config file and accept setting */
  char fn[256];
  sprintf(fn, "/etc/%s_filter_out_config", s->section);
  FILE *cfg = fopen(fn, "w");
  for (int i = 0; i < entries; i++) {
    fprintf(cfg, "%x %x\n", whitelist[i].id, whitelist[i].div);
  }
  fclose(cfg);

  strncpy(s->addr, val, s->len);

  return true;
}

int whitelists_init(void)
{
  static struct setting whitelist_settings[PORT_MAX];
  for (int i = 0; i < PORT_MAX; i++) {
    struct setting *s = &whitelist_settings[i];
    s->section = section_names[i];
    s->name = "enabled_sbp_messages";
    s->addr = wl[i];
    s->len = sizeof(wl[i]);
    s->notify = whitelist_notify;
    settings_register(s, TYPE_STRING);
  }
}
