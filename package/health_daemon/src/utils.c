/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <libsbp/sbp.h>
#include <string.h>

#include "utils.h"

static const char *const bool_names[] = { "False", "True" };
static const char *const section_acquisition = SETTING_SECTION_ACQUISITION;
static const char *const name_glonass_enabled =
  SETTING_GLONASS_ACQUISITION_ENABLED;

int health_util_parse_setting_read_resp(const u8 *msg,
                                        u8 msg_n,
                                        const char **section,
                                        const char **name,
                                        const char **value)
{
  const char **result_holders[] = { section, name, value };
  u8 start = 0;
  u8 end = 0;
  for (u8 i = 0; i < sizeof(result_holders) / sizeof(*result_holders); i++) {
    bool found = false;
    *(result_holders[i]) = NULL;
    while (end < msg_n) {
      if (msg[end] == '\0') {
        if (end == start) {
          return -1;
        } else {
          *(result_holders[i]) = (const char *)msg + start;
          start = (u8)(end + 1);
          found = true;
        }
      }
      end++;
      if (found) {
        break;
      }
    }
  }
  return 0;
}

int health_util_check_glonass_enabled(const char *section,
                                      const char *name,
                                      const char *value,
                                      bool *result)
{
  if (section == NULL || name == NULL || value == NULL) {
    return -1;
  }

  if (strcmp(section, section_acquisition) != 0
      || strcmp(name, name_glonass_enabled) != 0) {
    return -1;
  }

  if (strcmp(value, bool_names[(u8) true]) == 0) {
    *result = true;
  } else if (strcmp(value, bool_names[(u8) false]) == 0) {
    *result = false;
  } else {
    return -1;
  }

  return 0;
}
