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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#include <libpiksi/framer.h>

typedef struct framer_interface_s {
  const char *name;
  framer_create_fn_t create;
  framer_destroy_fn_t destroy;
  framer_process_fn_t process;
  struct framer_interface_s *next;
} framer_interface_t;

struct framer_s {
  void *state;
  framer_interface_t *interface;
};

static framer_interface_t *framer_interface_list = NULL;

static framer_interface_t * framer_interface_lookup(const char *name)
{
  framer_interface_t *interface;
  for (interface = framer_interface_list; interface != NULL;
       interface = interface->next) {
    if (strcasecmp(name, interface->name) == 0) {
      return interface;
    }
  }
  return NULL;
}

int framer_interface_register(const char *name,
                              framer_create_fn_t create,
                              framer_destroy_fn_t destroy,
                              framer_process_fn_t process)
{
  framer_interface_t *interface = (framer_interface_t *)
                                      malloc(sizeof(*interface));
  if (interface == NULL) {
    syslog(LOG_ERR, "error allocating framer interface");
    return -1;
  }

  *interface = (framer_interface_t) {
    .name = strdup(name),
    .create = create,
    .destroy = destroy,
    .process = process,
    .next = NULL
  };

  if (interface->name == NULL) {
    syslog(LOG_ERR, "error allocating framer interface members");
    free(interface);
    interface = NULL;
    return -1;
  }

  /* Add to list */
  framer_interface_t **p_next = &framer_interface_list;
  while (*p_next != NULL) {
    p_next = &(*p_next)->next;
  }
  *p_next = interface;

  return 0;
}

int framer_interface_valid(const char *name)
{
  framer_interface_t *interface = framer_interface_lookup(name);
  if (interface == NULL) {
    return -1;
  }
  return 0;
}

framer_t * framer_create(const char *name)
{
  /* Look up interface */
  framer_interface_t *interface = framer_interface_lookup(name);
  if (interface == NULL) {
    syslog(LOG_ERR, "unknown framer: %s", name);
    return NULL;
  }

  framer_t *framer = (framer_t *)malloc(sizeof(*framer));
  if (framer == NULL) {
    syslog(LOG_ERR, "error allocating framer");
    return NULL;
  }

  *framer = (framer_t) {
    .state = interface->create(),
    .interface = interface
  };

  if (framer->state == NULL) {
    syslog(LOG_ERR, "error creating framer");
    free(framer);
    framer = NULL;
    return NULL;
  }

  return framer;
}

void framer_destroy(framer_t **framer)
{
  (*framer)->interface->destroy(&(*framer)->state);
  free(*framer);
  *framer = NULL;
}

uint32_t framer_process(framer_t *framer,
                        const uint8_t *data, uint32_t data_length,
                        const uint8_t **frame, uint32_t *frame_length)
{
  return framer->interface->process(framer->state, data, data_length,
                                    frame, frame_length);
}

