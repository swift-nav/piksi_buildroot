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
extern "C" {
#include "metrics_daemon.h"
}
extern "C" struct json_object *  loop_through_folder_name(const char * process_path, const char * root, unsigned int root_len, struct json_object * json_root);
#define PROGRAM_NAME "metrics_daemon"
class MetricsDaemonTests : public ::testing::Test { };


// This is a unit level test verifying function static struct json_object *  loop_through_folder_name(const char * process_path, const char * root, unsigned int root_len, struct json_object * json_root)
// Function loop_through_folder_name traverse the existing json object by following the target file path and returns the least common path node as a json object
// the input root name is used to pair the json tree with the file path
TEST_F(MetricsDaemonTests, empty_ini_field) {

  const char * test_path_1 = "folder_layer_1/folder_layer_2/folder_layer_3/file_layer4";
  const char * root_name = "folder_layer_2";
  const char * node_1_name = "folder_layer_3";
  int root_len = 14;
  json_object * jobj_root = json_object_new_object();
  json_object * jobj_1 = json_object_new_object();
  json_object_object_add(jobj_root,root_name, jobj_1);
  json_object * jobj_2 = json_object_new_object();
  json_object_object_add(jobj_1,node_1_name, jobj_2);
  //current tree jobj_root->jobj_1(folder_layer_2)->jobj_2(folder_layer_3)
  json_object * jobj_ret = loop_through_folder_name(test_path_1, root_name, root_len, jobj_root);
  //expected result: jobj_2
  //below is the json-c documentation for function json_object_equal
  // /** Check if two json_object's are equal
  // *
  // * If the passed objects are equal 1 will be returned.
  // * Equality is defined as follows:
  // * - json_objects of different types are never equal
  // * - json_objects of the same primitive type are equal if the
  // *   c-representation of their value is equal
  // * - json-arrays are considered equal if all values at the same
  // *   indices are equal (same order)
  // * - Complex json_objects are considered equal if all
  // *   contained objects referenced by their key are equal,
  // *   regardless their order.
  // *
  // * @param obj1 the first json_object instance
  // * @param obj2 the second json_object instance
  // * @returns whether both objects are equal or not
  // */
   ASSERT_EQ(1, json_object_equal(jobj_ret,jobj_2));   // test case 1
   const char * test_path_2 = "folder_layer_1/folder_layer_2/file_layer3";
   jobj_ret = loop_through_folder_name(test_path_2, root_name, root_len, jobj_root);
   ASSERT_EQ(1, json_object_equal(jobj_ret,jobj_1));   // test case 2
   const char * node_2_name = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabc"; //node name with 80 bytes
   json_object * jobj_3 = json_object_new_object();
   json_object_object_add(jobj_2,node_2_name, jobj_3);
   //current tree jobj_root->jobj_1(folder_layer_2)->jobj_2(folder_layer_3)->jobj_3(80 bytes string)
   const char * test_path_3 = "folder_layer_1/folder_layer_2/folder_layer_3/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabc/"; // Note: put a slash here indicates it is a directory, as the rule ftw function returns
   jobj_ret = loop_through_folder_name(test_path_3, root_name, root_len, jobj_root);
   ASSERT_EQ(1, json_object_equal(jobj_ret,jobj_3));   // test case 3
   const char * test_path_4 = "folder_layer_1/folder_layer_2/folder_layer_3/abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabc"; // Note: name without slash here indicates it is a file, as the rule ftw function returns
   jobj_ret = loop_through_folder_name(test_path_4, root_name, root_len, jobj_root);
   ASSERT_EQ(1, json_object_equal(jobj_ret,jobj_2));   // test case 4: since the input is a file not a folder, it suppose to return upper level node here
   const char * test_path_5 = "folder_layer_1/folder_layer_2//file_layer3"; // double slash here to test invalid case
   jobj_ret = loop_through_folder_name(test_path_5, root_name, root_len, jobj_root);
   ASSERT_EQ(1, json_object_equal(jobj_ret,jobj_1));   // test case 4: expected result: node with key == folder_layer_2
   json_object_put(jobj_root);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  logging_init(PROGRAM_NAME);
  logging_log_to_stdout_only(true);
  auto ret = RUN_ALL_TESTS();
  logging_deinit();
  return ret;
}
