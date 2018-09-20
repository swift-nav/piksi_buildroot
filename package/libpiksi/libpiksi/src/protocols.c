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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <dlfcn.h>
#include <syslog.h>

#include <libpiksi/protocols.h>
#include <libpiksi/framer.h>
#include <libpiksi/filter.h>

#include "framer_none.h"
#include "filter_none.h"

#define DLSYM_CAST(var) (*(void **)&(var))

static int import_framer(const char *protocol_name, void *handle)
{
  framer_create_fn_t create_fn;
  DLSYM_CAST(create_fn) = dlsym(handle, "framer_create");
  if (create_fn == NULL) {
    return -1;
  }

  framer_destroy_fn_t destroy_fn;
  DLSYM_CAST(destroy_fn) = dlsym(handle, "framer_destroy");
  if (destroy_fn == NULL) {
    return -1;
  }

  framer_process_fn_t process_fn;
  DLSYM_CAST(process_fn) = dlsym(handle, "framer_process");
  if (process_fn == NULL) {
    return -1;
  }

  return framer_interface_register(protocol_name, create_fn, destroy_fn, process_fn);
}

static int import_filter(const char *protocol_name, void *handle)
{
  filter_create_fn_t create_fn;
  DLSYM_CAST(create_fn) = dlsym(handle, "filter_create");
  if (create_fn == NULL) {
    return -1;
  }

  filter_destroy_fn_t destroy_fn;
  DLSYM_CAST(destroy_fn) = dlsym(handle, "filter_destroy");
  if (destroy_fn == NULL) {
    return -1;
  }

  filter_process_fn_t process_fn;
  DLSYM_CAST(process_fn) = dlsym(handle, "filter_process");
  if (process_fn == NULL) {
    return -1;
  }

  return filter_interface_register(protocol_name, create_fn, destroy_fn, process_fn);
}

static const char *import_name(void *handle)
{
  const char **protocol_name;
  DLSYM_CAST(protocol_name) = dlsym(handle, "protocol_name");
  if (protocol_name == NULL) {
    return NULL;
  }
  return *protocol_name;
}

static int import(const char *filename)
{
  void *handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
  if (handle == NULL) {
    return -1;
  }

  const char *protocol_name = import_name(handle);
  if (protocol_name == NULL) {
    syslog(LOG_ERR, "missing protocol_name");
    goto error;
  }

  /* Note: framer and filter implementations are not mandatory */
  import_framer(protocol_name, handle);
  import_filter(protocol_name, handle);

  /* Do not close handle on success */
  return 0;

error:
  dlclose(handle);
  return -1;
}

int protocols_import(const char *path)
{
  /* Register "none" protocol */
  if (framer_interface_register("none",
                                framer_none_create,
                                framer_none_destroy,
                                framer_none_process)
      != 0) {
    syslog(LOG_ERR, "error registering none framer");
    return -1;
  }

  if (filter_interface_register("none",
                                filter_none_create,
                                filter_none_destroy,
                                filter_none_process)
      != 0) {
    syslog(LOG_ERR, "error registering none filter");
    return -1;
  }

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
      syslog(LOG_ERR, "error importing %s", filename);
    }
  }

  closedir(dir);

  return 0;
}
