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

int xxx, yyy;
u8 zzz;

int main(void)
{
  sbp_state_t *sbp = sbp_zmq_init();

  settings_setup(sbp);

  SETTING("demo", "xxx", xxx, TYPE_INT);
  SETTING("demo", "yyy", yyy, TYPE_INT);
  SETTING("demo", "zzz", zzz, TYPE_BOOL);

  sbp_zmq_loop(sbp);

  return 0;
}

