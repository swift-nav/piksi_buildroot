/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Dev <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <string>
#include <fstream>
#include <streambuf>
#include <iostream>
#include <json-c/json.h>
#include <gtest/gtest.h>
#include <libpiksi/logging.h>
#include "metrics_daemon.h"
#include "sbp.h"
#include <ftw.h>
extern json_object *jobj_root;
extern int handle_walk_path(const char *fpath, const struct stat *sb, int tflag);
extern void init_json_object(const char *path);
#define PROGRAM_NAME "metrics_daemon"
class MetricsDaemonTests : public ::testing::Test {
};


// This is a unit level test verifying function static struct json_object *
// loop_through_folder_name(const char * process_path, const char * root, unsigned int root_len,
// struct json_object * json_root) Function loop_through_folder_name traverse the existing json
// object by following the target file path and returns the least common path node as a json object
// the input root name is used to pair the json tree with the file path
TEST_F(MetricsDaemonTests, empty_ini_field)
{
  system("rm -rf /folder_layer_1");
  system("mkdir /folder_layer_1");
  system("mkdir /folder_layer_1/folder_layer_2");
  system("mkdir /folder_layer_1/folder_layer_2/folder_layer_3");
  system("touch /folder_layer_1/folder_layer_2/folder_layer_3/file_layer4"); // empty file
  system(
    "mkdir /folder_layer_1/folder_layer_2/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabc");
  system(
    "touch /folder_layer_1/folder_layer_2/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabc/double_data");
  system(
    "echo \"1.23456\" >> /folder_layer_1/folder_layer_2/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabc/double_data");

  const char *metrics_path = "/folder_layer_1/folder_layer_2";
  // current directory tree: folder_layer_2->folder_layer_3->file_layer4
  //                                       ->abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabc->double_data

  init_json_object(metrics_path);

  // variable initialization is identical to the code define in main()


  int ret = ftw(metrics_path, handle_walk_path, 20);
  ASSERT_NE(ret, -1); // test case 1: no error reported during traverse directory
  //  json_object * jobj_test = jobj_root;
  json_object_object_foreach(jobj_root, key, val)
  {
    if (strcmp(key, "folder_layer_3") == 0) {
      json_object *jobj_test_1 = val;
      json_object_object_foreach(jobj_test_1, key2, val2)
      {
        if (strcmp(key2, "file_layer4") == 0) // test case 2: empty file generate a json_null node
        {
          ASSERT_EQ(json_object_is_type(val2, json_type_null), true);
        }
      }
    } else if (
      strcmp(key,
             "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabc")
      == 0) {
      json_object *jobj_test_1 = val;
      json_object_object_foreach(jobj_test_1, key2, val2)
      {
        if (strcmp(key2, "double_data") == 0) {
          ASSERT_EQ(json_object_is_type(val2, json_type_double),
                    true); // test case 3: verify if the file that contains correct data generates a
                           // json double node
          ASSERT_DOUBLE_EQ(1.23456,
                           json_object_get_double(
                             val2)); // test case 4: verify the value of double is stored correct.
        }
      }
    }
  }

  system("rm -rf /folder_layer_1");
  json_object_put(jobj_root);


  int tflag = FTW_D;
  struct stat sb;
  const char *folder_path = "/folder_layer_1/folder_layer_2/folder3";

  init_json_object(metrics_path);
  handle_walk_path(folder_path, &sb, tflag);
  json_object_object_foreach(jobj_root, key3, val3)
  {
    if (strcmp(key, "folder3")
        == 0) // test case 5: simulate a folder and verify it generates a json_type_object node
    {
      ASSERT_EQ(json_object_is_type(val3, json_type_object), true);
    }
  }

  tflag = FTW_F;
  sb.st_size = 0;
  const char *file_path = "/folder_layer_1/folder_layer_2/file3";
  handle_walk_path(folder_path, &sb, tflag);
  json_object_object_foreach(jobj_root, key4, val4)
  {
    if (strcmp(key, "file3") == 0) // test case 6: empty file generate a json_null node
    {
      ASSERT_EQ(json_object_is_type(val4, json_type_null), true);
    }
  }

  json_object_put(jobj_root);
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);
  auto ret = RUN_ALL_TESTS();
  logging_deinit();
  return ret;
}
