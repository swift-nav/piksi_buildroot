/*
 * Copyright (C) 2012-2016 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_SBP_SETTINGS_H
#define SWIFTNAV_SBP_SETTINGS_H

#include <libsbp/sbp.h>

enum setting_types {
  TYPE_INT,
  TYPE_FLOAT,
  TYPE_STRING,
};
extern int TYPE_BOOL;

struct setting_type {
  int (*to_string)(const void *priv, char *str, int slen, const void *blob, int blen);
  bool (*from_string)(const void *priv, void *blob, int len, const char *str);
  int (*format_type)(const void *priv, char *str, int len);
  const void *priv;
  struct setting_type *next;
};

struct setting {
  const char *section;
  const char *name;
  void *addr;
  int len;
  bool (*notify)(struct setting *setting, const char *val);
  struct setting *next;
  const struct setting_type *type;
};

#define SETTING_NOTIFY(section, name, var, type, notify) do {         \
  static struct setting setting = \
    {(section), (name), &(var), sizeof(var), (notify), NULL, NULL}; \
  settings_register(&(setting), (type)); \
} while(0)

#define SETTING(section, name, var, type) \
  SETTING_NOTIFY(section, name, var, type, settings_default_notify)

#define READ_ONLY_PARAMETER(section, name, var, type) \
  SETTING_NOTIFY(section, name, var, type, settings_read_only_notify)

typedef int (*settings_msg_send_fn)(u16 msg_type, u8 len, u8 *payload);
typedef int (*settings_msg_cb_register_fn)(u16 msg_type,
                                           sbp_msg_callback_t cb, void *context,
                                           sbp_msg_callbacks_node_t **node);
typedef int (*settings_msg_cb_remove_fn)(sbp_msg_callbacks_node_t *node);
typedef int (*settings_msg_loop_timeout_fn)(u32 timeout_ms);
typedef int (*settings_msg_loop_interrupt_fn)(void);

typedef struct {
  settings_msg_send_fn msg_send;
  settings_msg_cb_register_fn msg_cb_register;
  settings_msg_cb_remove_fn msg_cb_remove;
  settings_msg_loop_timeout_fn msg_loop_timeout;
  settings_msg_loop_interrupt_fn msg_loop_interrupt;
} settings_interface_t;

void settings_setup(const settings_interface_t *interface);
int settings_type_register_enum(const char * const enumnames[], struct setting_type *type);
void settings_register(struct setting *s, enum setting_types type);
bool settings_default_notify(struct setting *setting, const char *val);
bool settings_read_only_notify(struct setting *setting, const char *val);

#endif  /* SWIFTNAV_SBP_SETTINGS_H */

