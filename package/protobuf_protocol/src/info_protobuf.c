/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
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

const char *protocol_name = "PROTOBUF";
const char *setting_name = "PROTOBUF";

int port_adapter_opts_get(char *buf, size_t buf_size, const char *port_name)
{
  return snprintf(buf, buf_size,
                  "-f protobuf "
                  "-p >tcp://127.0.0.1:46031 "
                  "-s >tcp://127.0.0.1:46030",
                  port_name);
}
