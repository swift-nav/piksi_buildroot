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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#include <libpiksi/filter.h>

typedef struct filter_interface_s {
  const char *name;
  filter_create_fn_t create;
  filter_destroy_fn_t destroy;
  filter_process_fn_t process;
  struct filter_interface_s *next;
} filter_interface_t;

struct filter_s {
  void *state;
  filter_interface_t *interface;
};

static filter_interface_t *filter_interface_list = NULL;

static filter_interface_t *filter_interface_lookup(const char *name)
{
  filter_interface_t *interface;
  for (interface = filter_interface_list; interface != NULL; interface = interface->next) {
    if (strcasecmp(name, interface->name) == 0) {
      return interface;
    }
  }
  return NULL;
}

int filter_interface_register(const char *name,
                              filter_create_fn_t create,
                              filter_destroy_fn_t destroy,
                              filter_process_fn_t process)
{
  filter_interface_t *interface = (filter_interface_t *)malloc(sizeof(*interface));
  if (interface == NULL) {
    syslog(LOG_ERR, "error allocating filter interface");
    return -1;
  }

  *interface = (filter_interface_t){
    .name = strdup(name),
    .create = create,
    .destroy = destroy,
    .process = process,
    .next = NULL,
  };

  if (interface->name == NULL) {
    syslog(LOG_ERR, "error allocating filter interface members");
    free(interface);
    interface = NULL;
    return -1;
  }

  /* Add to list */
  filter_interface_t **p_next = &filter_interface_list;
  while (*p_next != NULL) {
    p_next = &(*p_next)->next;
  }
  *p_next = interface;

  return 0;
}

int filter_interface_valid(const char *name)
{
  filter_interface_t *interface = filter_interface_lookup(name);
  if (interface == NULL) {
    return -1;
  }
  return 0;
}

filter_t *filter_create(const char *name, const char *filename)
{
  /* Look up interface */
  filter_interface_t *interface = filter_interface_lookup(name);
  if (interface == NULL) {
    syslog(LOG_ERR, "unknown filter: %s", name);
    return NULL;
  }

  filter_t *filter = (filter_t *)malloc(sizeof(*filter));
  if (filter == NULL) {
    syslog(LOG_ERR, "error allocating filter");
    return NULL;
  }

  *filter = (filter_t){.state = interface->create(filename), .interface = interface};

  if (filter->state == NULL) {
    syslog(LOG_ERR, "error creating filter");
    free(filter);
    filter = NULL;
    return NULL;
  }

  return filter;
}

void filter_destroy(filter_t **filter)
{
  (*filter)->interface->destroy(&(*filter)->state);
  free(*filter);
  *filter = NULL;
}

int filter_process(filter_t *filter, const uint8_t *msg, uint32_t msg_length)
{
  return filter->interface->process(filter->state, msg, msg_length);
}
