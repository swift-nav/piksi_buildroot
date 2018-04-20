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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char *protocol_name = "SBP";
const char *setting_name = "SBP";

int port_adapter_opts_get(char *buf, size_t buf_size, const char *port_name)
{
  return snprintf(buf, buf_size,
                  "-f sbp "
                  "--filter-out sbp "
                  "--filter-out-config /etc/filter_out_config/%s "
                  "-p '>tcp://127.0.0.1:43031' "
                  "-s '>tcp://127.0.0.1:43030'",
                  port_name);
}
