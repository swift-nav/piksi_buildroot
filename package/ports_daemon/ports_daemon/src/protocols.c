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

#include "protocols.h"
#include <libpiksi/logging.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <dlfcn.h>

#define DLSYM_CAST(var) (*(void **)&(var))

typedef struct protocol_list_element_s {
  protocol_t protocol;
  struct protocol_list_element_s *next;
} protocol_list_element_t;

static protocol_list_element_t *protocol_list = NULL;

static const char *import_string(void *handle, const char *sym)
{
  const char **str;
  DLSYM_CAST(str) = dlsym(handle, sym);
  if (str == NULL) {
    return NULL;
  }
  return *str;
}

static int import(const char *filename)
{
  void *handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);

  const char *protocol_name = import_string(handle, "protocol_name");
  if (protocol_name == NULL) {
    piksi_log(LOG_ERR, "missing protocol_name");
    goto error;
  }

  const char *setting_name = import_string(handle, "setting_name");
  if (setting_name == NULL) {
    piksi_log(LOG_ERR, "missing setting_name");
    goto error;
  }

  port_adapter_opts_get_fn_t port_adapter_opts_get;
  DLSYM_CAST(port_adapter_opts_get) = dlsym(handle, "port_adapter_opts_get");
  if (port_adapter_opts_get == NULL) {
    piksi_log(LOG_ERR, "missing port_adapter_opts_get()");
    goto error;
  }

  protocol_list_element_t *e = (protocol_list_element_t *)malloc(sizeof(*e));
  if (e == NULL) {
    piksi_log(LOG_ERR, "error allocating protocol list element");
    goto error;
  }

  *e = (protocol_list_element_t){.protocol = {.name = protocol_name,
                                              .setting_name = setting_name,
                                              .port_adapter_opts_get = port_adapter_opts_get},
                                 .next = NULL};

  /* Add to list */
  protocol_list_element_t **p_next = &protocol_list;
  while (*p_next != NULL) {
    p_next = &(*p_next)->next;
  }
  *p_next = e;

  /* Do not close handle on success */
  return 0;

error:
  dlclose(handle);
  return -1;
}

int protocols_import(const char *path)
{
  /* Load protocols from libraries */
  DIR *dir = opendir(path);
  if (dir == NULL) {
    return -1;
  }

  struct dirent *dirent;
  while ((dirent = readdir(dir)) != NULL) {
    if (dirent->d_type != DT_REG) {
      continue;
    }

    char filename[PATH_MAX];
    snprintf(filename, sizeof(filename), "%s/%s", path, dirent->d_name);
    if (import(filename) != 0) {
      piksi_log(LOG_ERR, "error importing %s", filename);
    }
  }

  closedir(dir);

  return 0;
}

int protocols_count_get(void)
{
  int count = 0;
  protocol_list_element_t *e;
  for (e = protocol_list; e != NULL; e = e->next) {
    count++;
  }
  return count;
}

const protocol_t *protocols_get(int index)
{
  int offset = 0;
  protocol_list_element_t *e;
  for (e = protocol_list; e != NULL; e = e->next) {
    if (offset == index) {
      return &e->protocol;
    }
    offset++;
  }
  return NULL;
}
