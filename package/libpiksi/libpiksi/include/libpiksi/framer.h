/*
 * Copyright (C) 2016-2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_FRAMER_H
#define SWIFTNAV_FRAMER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct framer_s framer_t;

typedef void *(*framer_create_fn_t)(void);
typedef void (*framer_destroy_fn_t)(void **state);
typedef uint32_t (*framer_process_fn_t)(void *state,
                                        const uint8_t *data,
                                        uint32_t data_length,
                                        const uint8_t **frame,
                                        uint32_t *frame_length);

int framer_interface_register(const char *name,
                              framer_create_fn_t create,
                              framer_destroy_fn_t destroy,
                              framer_process_fn_t process);
int framer_interface_valid(const char *name);

framer_t *framer_create(const char *name);
void framer_destroy(framer_t **framer);
uint32_t framer_process(framer_t *framer,
                        const uint8_t *data,
                        uint32_t data_length,
                        const uint8_t **frame,
                        uint32_t *frame_length);

#endif /* SWIFTNAV_FRAMER_H */
