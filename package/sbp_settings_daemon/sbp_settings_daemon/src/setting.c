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

#include <string.h>

#include <libpiksi/min_ini.h>

#include <internal/setting.h>

static setting_t *settings_head;

/* Register a new setting in our linked list */
settings_reg_res_t setting_register(setting_t *new_setting)
{
  /* iterator */
  setting_t *it_setting;

  if (settings_head == NULL) {
    settings_head = new_setting;
  } else {
    for (it_setting = settings_head; it_setting->next; it_setting = it_setting->next) {
      if ((strcmp(it_setting->section, new_setting->section) == 0)
          && (strcmp(it_setting->next->section, new_setting->section) != 0))
        break;
    }
    new_setting->next = it_setting->next;
    it_setting->next = new_setting;
  }

  const char *default_value = "{2F9D26FF-F64C-4F9F-94FE-AE9F57758835}";
  char buf[SETTINGS_BUFLEN] = {0};

  ini_gets(new_setting->section, new_setting->name, default_value, buf, sizeof(buf), SETTINGS_FILE);

  if (strcmp(buf, default_value) != 0) {
    /* Use value from config file */
    strncpy(new_setting->value, buf, sizeof(new_setting->value));
    new_setting->dirty = true;
    return SETTINGS_REG_OK_PERM;
  }

  return SETTINGS_REG_OK;
}

/* Lookup setting in our linked list */
setting_t *setting_lookup(const char *section, const char *name)
{
  for (setting_t *it = settings_head; it; it = it->next) {
    if ((strcmp(it->section, section) == 0) && (strcmp(it->name, name) == 0)) {
      return it;
    }
  }
  return NULL;
}

setting_t *setting_find_by_index(u16 index)
{
  setting_t *sdata = settings_head;
  u16 i = 0;

  while ((i < index) && (sdata != NULL)) {
    sdata = sdata->next;
    i++;
  }

  return sdata;
}
