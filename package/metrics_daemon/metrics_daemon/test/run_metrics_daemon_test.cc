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
extern char *root_name;
extern unsigned int root_length;
#define PROGRAM_NAME "metrics_daemon"

class MetricsDaemonTests : public ::testing::Test {

 protected:
  void TearDown() override
  {
    ASSERT_TRUE(json_object_put(jobj_root) == 1);
    jobj_root = nullptr;
  }
};

// This is a unit level test verifying function static struct json_object *
// loop_through_folder_name(const char * process_path, const char * root, unsigned int root_len,
// struct json_object * json_root) Function loop_through_folder_name traverse the existing json
// object by following the target file path and returns the least common path node as a json object
// the input root name is used to pair the json tree with the file path
TEST_F(MetricsDaemonTests, metrics_update)
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
  root_name = extract_filename(metrics_path);
  root_length = strlen(root_name);
  bool found_file_layer4 = false;
  bool found_double_data = false;

  // variable initialization is identical to the code define in main()
  init_json_object(root_name);

  int ret = ftw(metrics_path, handle_walk_path, 20);
  ASSERT_NE(ret, -1); // test case 1: no error reported during traverse directory
  json_object_object_foreach(jobj_root, root_key, root_val)
  {
    if (strcmp(root_key, "folder_layer_2") == 0) {
      json_object_object_foreach(root_val, key, val)
      {
        if (strcmp(key, "folder_layer_3") == 0) {
          json_object *jobj_test_1 = val;
          json_object_object_foreach(jobj_test_1, key2, val2)
          {
            if (strcmp(key2, "file_layer4")
                == 0) // test case 2: empty file generate a json_null node
            {
              ASSERT_EQ(json_object_is_type(val2, json_type_null), true);
              found_file_layer4 = true;
            }
          }
        } else if (
          strcmp(
            key,
            "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabc")
          == 0) {
          json_object *jobj_test_1 = val;
          json_object_object_foreach(jobj_test_1, key2, val2)
          {
            if (strcmp(key2, "double_data") == 0) {
              ASSERT_EQ(json_object_is_type(val2, json_type_double),
                        true); // test case 3: verify if the file that contains correct data
                               // generates a json double node
              ASSERT_DOUBLE_EQ(1.23456,
                               json_object_get_double(val2)); // test case 4: verify the value of
                                                              // double is stored correct.
              found_double_data = true;
            }
          }
        }
      }
    }
  }
  ASSERT_TRUE(found_file_layer4);
  ASSERT_TRUE(found_double_data);
  system("rm -rf /folder_layer_1");
  json_object_put(jobj_root);

  int tflag = FTW_D;
  struct stat sb;
  const char *folder_path = "/folder_layer_1/folder_layer_2/folder3";

  bool found_folder3 = false;

  init_json_object(root_name);
  handle_walk_path(folder_path, &sb, tflag);
  json_object_object_foreach(jobj_root, root_key_2, root_val_2)
  {
    if (strcmp(root_key_2, "folder_layer_2") == 0) {
      json_object_object_foreach(root_val_2, key3, val3)
      {
        if (strcmp(key3, "folder3")
            == 0) // test case 5: simulate a folder and verify it generates a json_type_object node
        {
          ASSERT_EQ(json_object_is_type(val3, json_type_object), true);
          found_folder3 = true;
        }
      }
    }
  }


  ASSERT_TRUE(found_folder3);

  tflag = FTW_F;
  sb.st_size = 0;
  const char *file_path = "/folder_layer_1/folder_layer_2/file3";
  handle_walk_path(file_path, &sb, tflag);
  bool found_file3 = false;
  json_object_object_foreach(jobj_root, root_key_3, root_val_3)
  {
    if (strcmp(root_key_3, "folder_layer_2") == 0) {
      json_object_object_foreach(root_val_3, key4, val4)
      {
        if (strcmp(key4, "file3") == 0) // test case 6: empty file generate a json_null node
        {
          ASSERT_EQ(json_object_is_type(val4, json_type_null), true);
          found_file3 = true;
        }
      }
    }
  }
  ASSERT_TRUE(found_file3);
}

TEST_F(MetricsDaemonTests, loop_through_folder_name)
{
  ASSERT_EQ(jobj_root, nullptr);

  char json_file[] = "json_temp-XXXXXX";
  int fd_json = mkstemp(json_file);

  ASSERT_NE(fd_json, -1);
  ASSERT_EQ(unlink(json_file), 0);

  FILE *fp_json = fdopen(fd_json, "w+");

  char json_buf[] = "{ \"this\" : { \"is\" : { \"a\" : { \"metric\" : 123.0 } } } }";

  ASSERT_GE(fprintf(fp_json, "%s", json_buf), 0);

  fseek(fp_json, 0, SEEK_SET);
  jobj_root = json_object_from_fd(fd_json);

  ASSERT_NE(jobj_root, nullptr);

  const char *root = "this";
  const char *process_path = "this/is/a/metric";

  json_object *jobj_cur = loop_through_folder_name(process_path, root, strlen(root));
  ASSERT_NE(jobj_cur, nullptr);

  const char *process_path_2 = "this/is/b";

  json_object *jobj_cur_2 = loop_through_folder_name(process_path_2, root, strlen(root));
  ASSERT_NE(jobj_cur, nullptr);
  bool found_root = false;
  bool found_key = false;
  json_object_object_foreach(jobj_root, key, val)
  {
    if (strcmp(key, "this") == 0) {
      ASSERT_EQ(json_object_is_type(val, json_type_object), true);
      found_root = true;
      json_object_object_foreach(val, key2, val2)
      {
        if (strcmp(key2, "is") == 0) {
          json_object_object_foreach(val2, key3, val3)
          {
            if (strcmp(key3, "a")
                == 0) { // loop_through_folder_name returns a least common node of the basename
                        // so for string this/is/a/metric it should return json_object of 'a'
              found_key = true;
              ASSERT_EQ(json_object_equal(jobj_cur, val3), 1);
            }
          }
          ASSERT_EQ(json_object_equal(jobj_cur_2, val2),
                    1); // for string "this/is/b" it should return json_object of 'is'
        }
      }
    }
  }
  ASSERT_TRUE(found_root);
  ASSERT_TRUE(found_key);
  fclose(fp_json);
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
