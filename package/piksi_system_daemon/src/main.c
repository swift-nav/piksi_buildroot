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

#include "settings.h"
#include "sbp_zmq.h"

int uart0_baudrate = 115200;
int uart1_baudrate = 115200;

bool baudrate_notify(struct setting *s, const char *val)
{
  int baudrate;
  bool ret = s->type->from_string(s->type->priv, &baudrate, s->len, val);
  if (!ret) {
    return false;
  }

  const char *dev = NULL;
  if (s->addr == &uart0_baudrate) {
    dev = "/dev/ttyPS0";
  } else if (s->addr == &uart1_baudrate) {
    dev = "/dev/ttyPS1";
  } else {
    return false;
  }
  char cmd[80];
  snprintf(cmd, sizeof(cmd), "stty -F %s %d", dev, baudrate);
  ret = system(cmd) == 0;

  if (ret) {
    *(int*)s->addr = baudrate;
  }
  return ret;
}

int main(void)
{
  sbp_state_t *sbp = sbp_zmq_init();

  settings_setup(sbp);

  SETTING_NOTIFY("uart", "uart0_baudrate", uart0_baudrate, TYPE_INT, baudrate_notify);
  SETTING_NOTIFY("uart", "uart1_baudrate", uart1_baudrate, TYPE_INT, baudrate_notify);

  sbp_zmq_loop(sbp);

  return 0;
}
