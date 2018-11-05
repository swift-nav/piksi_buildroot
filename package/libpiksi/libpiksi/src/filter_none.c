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

#include "filter_none.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  void *dummy;
} filter_none_state_t;

void *filter_none_create(const char *filename)
{
  (void)filename;

  filter_none_state_t *s = (filter_none_state_t *)malloc(sizeof(*s));
  if (s == NULL) {
    return NULL;
  }

  return (void *)s;
}

void filter_none_destroy(void **state)
{
  free(*state);
  *state = NULL;
}

int filter_none_process(void *state, const uint8_t *msg, uint32_t msg_length)
{
  (void)state;
  (void)msg;
  (void)msg_length;

  return 0;
}
