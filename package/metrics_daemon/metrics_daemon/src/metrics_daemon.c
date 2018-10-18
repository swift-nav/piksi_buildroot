/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */
#define _GNU_SOURCE
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>
#include <json-c/json.h>
#include <string.h>
#include <libpiksi/logging.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>
#include <libpiksi/settings.h>
#include "sbp.h"
#include "metrics_daemon.h"
#define PROGRAM_NAME "metrics_daemon"

#define METRICS_ROOT_DIRECTORY "/var/log/metrics"
#define METRICS_OUTPUT_FILENAME "/var/log/metrics.json"
#define METRICS_USAGE_UPDATE_INTERVAL_MS (1000u)

#define MAX_FOLDER_NAME_LENGTH 64
static json_object *jobj_cur = NULL;
json_object *jobj_root = NULL;
char *root_name = NULL;
unsigned int root_length = 0;
static bool first_folder = true;
static const char *target_file = METRICS_OUTPUT_FILENAME;
static const char *metrics_path = METRICS_ROOT_DIRECTORY;
static bool enable_log_to_file = false;
static unsigned int metrics_update_interval = 1;


/**
 * @brief function updates json to file
 *
 * @param root       pointer to the root of json object
 * @param file_name  file to write to
 */
static bool write_json_to_file(struct json_object *root, const char *file_path)
{
  return file_append_string(file_path, json_object_to_json_string(root));
}

/**
 * @brief static function that returns file name from path
 *
 * @param  str        file path in ASCII
 * @return file_name  file name
 */
char *extract_filename(const char *str)
{
  return basename((char *)str);
}


/**
 * @brief function that use the full file path loop through the json object tree
 * to update values
 *
 * @param process_path file path in ASCII
 * @param root         root name, this is used to pair with the root of the json
 * object tree
 * @param root_len     number of chars for the root name
 * @return json object node that needs to be updated
 */
struct json_object *loop_through_folder_name(const char *process_path,
                                             const char *root,
                                             unsigned int root_len)
{
  char slash = '/';
  const char *start_ptr = NULL;
  bool found_root = false;
  start_ptr = process_path;
  if (start_ptr == NULL) {
    return jobj_root;
  }
  unsigned int str_len = (unsigned int)strlen(process_path);
  char *end_ptr = NULL;
  struct json_object *json_current = jobj_root;
  while (start_ptr != NULL) {
    if (end_ptr != NULL) {
      start_ptr++;
      if (start_ptr > process_path + str_len) return json_current;
    }
    end_ptr = strchr(start_ptr, slash); // search the second slash
    if (end_ptr != NULL) {

      unsigned int substr_len =
        (unsigned)(end_ptr - start_ptr); // cast to unsigned since the negative and zero
                                         // case is handled by the if statement
      if (!found_root && strncmp(start_ptr, root, root_len) == 0) // search the root first
      {
        found_root = true;
        start_ptr--;
        continue;
      } else if (found_root) {
        bool found_target = false;
        json_object_object_foreach(json_current,
                                   key,
                                   val) // for each subfolder, search the name as key in json tree
        {
          if (strncmp((start_ptr), key, substr_len) == 0) // if find the folder name, continue on
                                                          // the loop with the new current node
          {
            json_current = val;
            found_target = true;
            break;
          }
        }
        if (!found_target) // if couldn't find target on the tree, return
                           // current node to add new node
          return json_current;
      }
    }
    start_ptr = end_ptr; // if there is still sub-folders, update the start_ptr to end_ptr
  }
  return json_current;
}


/**
 * @brief function get called by ftw for each of the sub-folder or file under
 * the path
 *
 * @param fpath  file path in ASCII
 * @param sb     file status struct
 * @param tflag  flag indicates file/folder type
 * @return integer that indicates if the walk through failed.
 */
int handle_walk_path(const char *fpath, const struct stat *sb, int tflag)
{
  if (first_folder == true) // skip the root since it is handled by main()
  {
    first_folder = false;
    return 0;
  }
  char *bname = extract_filename(fpath);
  if (bname == NULL) {
    piksi_log(LOG_ERR, "FTW: invalid file name\n"); // For now, only support file and forlder
    return -1;
  }
  json_object *jobj_lc_node =
    loop_through_folder_name(fpath,     // get the least common node between json object and file
                             root_name, // path that needs to be updated
                             root_length);

  if (jobj_lc_node == NULL) {
    return -1;
  }

  switch (tflag) {
  case FTW_D: {
    bool file_entry_exist = false;
    json_object_object_foreach(jobj_lc_node, key, val)
    {
      if (strcmp(bname, key) == 0) {
        file_entry_exist = true;
        jobj_cur = val;
        break;
      }
    }
    if (!file_entry_exist) // if it is a folder and it doesn't exist in json
                           // tree, add a new folder, do nothing otherwise
    {
      json_object *jnewobj = json_object_new_object(); // Create a new object here to set the name
                                                       // of folder
      json_object_object_add(jobj_lc_node, bname, jnewobj);
    }
    break;
  }

  case FTW_F: {
    char buf[64] = {0};
    int file_len = (int)(sb->st_size - 1);
    double number = 0.0;
    if (file_len < 64 && file_len > 0) {
      FILE *fp = fopen(fpath, "r"); // read file to get the data
      if (fp == NULL) {
        piksi_log(LOG_ERR, "error opening %s : %s", fpath, strerror(errno));
        return -1;
      }
      fread(&buf[0], (unsigned int)file_len, 1, fp);
      fclose(fp);
      number = atof(&buf[0]);
    } else {
      file_len = 0; // if file is empty, set the str length to zero
    }

    bool file_entry_exist = false;
    json_object_object_foreach(jobj_lc_node, key, val)
    {
      if (strcmp(bname, key) == 0) {
        file_entry_exist = true;
        if (file_len > 0) {
          if (json_object_is_type(val, json_type_double))
            json_object_set_double(val, number); // if file exist, update
          else if (json_object_is_type(val, json_type_null)) {
            json_object *jobj_remove = json_object_get(
              val); // need to get the ownership of the json_object node before call put
            json_object_put(jobj_remove);
            json_object_object_add(jobj_lc_node, bname, json_object_new_double(number));
          }
        } else {
          json_object *jobj_remove = json_object_get(
            val); // need to get the ownership of the json_object node before call put
          json_object_put(jobj_remove);
          json_object_object_add(jobj_lc_node, key, NULL);
        }
        break;
      }
    }
    if (!file_entry_exist) // add a new node if it doesn't exist
    {
      if (file_len == 0)
        json_object_object_add(jobj_lc_node, bname, NULL);
      else
        json_object_object_add(jobj_lc_node, bname, json_object_new_double(number));
    }
    break;
  }
  default: {
    piksi_log(LOG_ERR,
              "FTW: unexpected file type\n"); // For now, only support file and forlder
    return -1;
  }
  }

  return 0;
}

/**
 * @brief function that write json to file
 *
 * Write messaging metrics to file
 */
static void write_metrics_to_file()
{
  // Walk dir for metrics
  first_folder = true;
  if (enable_log_to_file) {
    if (ftw(metrics_path, handle_walk_path, 20) == -1) {
      return;
    }
    if (!write_json_to_file(jobj_root, target_file)) piksi_log(LOG_ERR, "Failed to write to file");
  }
}

static int parse_options(int argc, char *argv[])
{
  enum { OPT_ID_METRICS_PATH = 1 };

  const struct option long_opts[] = {
    {"path", required_argument, 0, OPT_ID_METRICS_PATH},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_METRICS_PATH: {
      metrics_path = optarg;
      break;
    }
    default: {
      puts("Invalid option");
      return -1;
    }
    }
  }

  return 0;
}

/**
 * @brief used to trigger usage updates
 */
static void run_routine_function(pk_loop_t *loop, void *timer_handle, void *context)
{
  (void)loop;
  (void)timer_handle;
  (void)context;
  write_metrics_to_file();
}


static int cleanup(int status)
{
  logging_deinit();
  sbp_deinit();
  /**
   * Decrement the reference count of json_object and free if it reaches zero.
   * You must have ownership of obj prior to doing this or you will cause an
   * imbalance in the reference count.
   * An obj of NULL may be passed; in that case this call is a no-op.
   *
   * @param obj the json_object instance
   * @returns 1 if the object was freed.
   */
  json_object *jobj_remove =
    json_object_get(jobj_root); // need to get the ownership of the json_object node before call put
  if (json_object_put(jobj_remove) != 1) piksi_log(LOG_ERR, "failed to free json object");
  return status;
}

void init_json_object(const char *name)
{
  jobj_root = json_object_new_object();
  json_object_object_add(jobj_root, name, json_object_new_object());
}

static int notify_log_settings_changed(void *context)
{
  (void)context;
  piksi_log(LOG_DEBUG | LOG_SBP,
            "Settings changed: enable_log_to_file = %d metrics_update_interval = %d",
            enable_log_to_file,
            metrics_update_interval);
  sbp_update_timer_interval(metrics_update_interval * METRICS_USAGE_UPDATE_INTERVAL_MS,
                            run_routine_function);
  return 0;
}

int main(int argc, char *argv[])
{

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    return cleanup(EXIT_FAILURE);
  }
  root_name = extract_filename(metrics_path); // for the file name, clearn it up in cleanup
  root_length = (unsigned int)strlen(root_name);
  init_json_object(root_name);
  assert(jobj_root != NULL);
  if (sbp_init(METRICS_USAGE_UPDATE_INTERVAL_MS, run_routine_function) != 0) {
    piksi_log(LOG_ERR | LOG_SBP, "Error initializing SBP!");
    return cleanup(EXIT_FAILURE);
  }

  settings_ctx_t *settings_ctx = sbp_get_settings_ctx();

  settings_register(settings_ctx,
                    "metrics_daemon",
                    "enable_log_to_file",
                    &enable_log_to_file,
                    sizeof(enable_log_to_file),
                    SETTINGS_TYPE_BOOL,
                    notify_log_settings_changed,
                    NULL);
  settings_register(settings_ctx,
                    "metrics_daemon",
                    "metrics_update_interval",
                    &metrics_update_interval,
                    sizeof(metrics_update_interval),
                    SETTINGS_TYPE_INT,
                    notify_log_settings_changed,
                    NULL);
  sbp_run();

  piksi_log(LOG_DEBUG, "Metrics Daemon: Normal Exit");


  return cleanup(EXIT_SUCCESS);
}
