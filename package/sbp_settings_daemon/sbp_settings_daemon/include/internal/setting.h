/*
 * Copyright (C) 2019 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SBP_SETTINGS_DAEMON_SETTING_H
#define SBP_SETTINGS_DAEMON_SETTING_H

#include <libsettings/settings.h>

#define SETTINGS_FILE "/persistent/config.ini"

typedef struct setting_s {
  char section[SETTINGS_BUFLEN];
  char name[SETTINGS_BUFLEN];
  char value[SETTINGS_BUFLEN];
  char type[SETTINGS_BUFLEN];
  struct setting_s *next;
  bool dirty;
} setting_t;

#ifdef __cplusplus
extern "C" {
#endif

settings_reg_res_t setting_register(setting_t *new_setting);
setting_t *setting_lookup(const char *section, const char *name);
setting_t *setting_find_by_index(u16 index);

#ifdef __cplusplus
}
#endif

#endif /* SBP_SETTINGS_DAEMON_SETTING_H */
