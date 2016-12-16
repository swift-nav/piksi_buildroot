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
#include <libsbp/piksi.h>

#include "sbp_zmq.h"
#include "settings.h"
#include "sbp_fileio.h"

static void reset_callback(u16 sender_id, u8 len, u8 msg_[], void* context)
{
  (void)sender_id; (void) context;

  /* Reset settings to defaults if requested */
  if (len == sizeof(msg_reset_t)) {
    const msg_reset_t *msg = (const void*)msg_;
    if (msg->flags & 1) {
      settings_reset_defaults();
    }
  }

  /* We use -f to force immediate reboot.  Orderly shutdown sometimes fails
   * when unloading remoteproc drivers. */
  system("reboot -f");
}

int main(void)
{
  sbp_state_t *sbp = sbp_zmq_init();

  sbp_zmq_register_callback(sbp, SBP_MSG_RESET,
                            reset_callback);
  sbp_zmq_register_callback(sbp, SBP_MSG_RESET_DEP,
                            reset_callback);

  settings_setup(sbp);
  sbp_fileio_setup(sbp);

  sbp_zmq_loop(sbp);

  return 0;
}

