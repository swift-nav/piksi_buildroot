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

#ifndef SWIFTNAV_FILTER_H
#define SWIFTNAV_FILTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FRAMER_NONE_NAME "none"
#define FILTER_NONE_NAME "none"
#define FILTER_SWL_NAME "settings_whitelist"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct filter_list_s filter_list_t;

typedef void* (*filter_create_fn_t)(const char *filename);
typedef void (*filter_destroy_fn_t)(void **state);
typedef int (*filter_process_fn_t)(void *state, const uint8_t *msg,
                                   uint32_t msg_length);

int filter_interface_register(const char *name,
                              filter_create_fn_t create,
                              filter_destroy_fn_t destroy,
                              filter_process_fn_t process);
int filter_interface_valid(const char *name);

typedef struct {
  const char* name;
  const char* filename;
} filter_spec_t;

filter_list_t* filter_create(filter_spec_t filter_specs[], size_t spec_count);
void filter_destroy(filter_list_t **filter_list);
int filter_process(filter_list_t *filter_list, const uint8_t *msg, uint32_t msg_length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* SWIFTNAV_FILTER_H */
