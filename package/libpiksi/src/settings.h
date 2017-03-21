/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef LIBPIKSI_SETTINGS_H
#define LIBPIKSI_SETTINGS_H

#include "common.h"

typedef int settings_type_t;

enum {
  SETTINGS_TYPE_INT,
  SETTINGS_TYPE_FLOAT,
  SETTINGS_TYPE_STRING,
  SETTINGS_TYPE_BOOL
};

typedef int (*settings_notify_fn)(void *context);

typedef struct settings_ctx_s settings_ctx_t;

settings_ctx_t * settings_create(void);
void settings_destroy(settings_ctx_t **ctx);

int settings_type_register_enum(settings_ctx_t *ctx,
                                const char * const enum_names[],
                                settings_type_t *type);
int settings_register(settings_ctx_t *ctx, const char *section,
                      const char *name, void *var, size_t var_len,
                      settings_type_t type, settings_notify_fn notify,
                      void *notify_context);
int settings_register_readonly(settings_ctx_t *ctx, const char *section,
                               const char *name, const void *var,
                               size_t var_len, settings_type_t type);

int settings_read(settings_ctx_t *ctx);
int settings_pollitem_init(settings_ctx_t *ctx, zmq_pollitem_t *pollitem);
int settings_pollitem_check(settings_ctx_t *ctx, zmq_pollitem_t *pollitem);
int settings_reader_add(settings_ctx_t *ctx, zloop_t *zloop);
int settings_reader_remove(settings_ctx_t *ctx, zloop_t *zloop);

#endif /* LIBPIKSI_SETTINGS_H */
