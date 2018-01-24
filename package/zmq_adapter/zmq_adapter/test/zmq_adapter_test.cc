/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Engineering <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <gtest/gtest.h>

#include "filter_settings_whitelist.h"
#include "filter_settings_whitelist.c"

class ZmqAdapterTest : public ::testing::Test { };

TEST_F(ZmqAdapterTest, NormalOperation) {

  const char *vector[] = {"net.ip", "net.addr", "net.mask"};
  unsigned int nkeys = 3;

  cmph_io_adapter_t *source = cmph_io_vector_adapter((char **)vector, nkeys);

  cmph_config_t *config = cmph_config_new(source);
  cmph_config_set_algo(config, CMPH_BDZ);
  cmph_t *hash = cmph_new(config);
  cmph_config_destroy(config);

  char whitelist[] = 
    "net.ip\n"
    "net.addr\n"
    "net.mask\n";

  ssize_t entry_count = collate_whitelist(whitelist, sizeof(whitelist) - 1, NULL, NULL);

  ASSERT_GT(entry_count, 0);
  ASSERT_EQ(entry_count, 3);

  whitelist_entry_t* entries = (whitelist_entry_t*) malloc(entry_count*sizeof(*entries));

  entry_count = collate_whitelist(whitelist, sizeof(whitelist) - 1, entries, hash);
  ASSERT_EQ(entry_count, 3);

  ASSERT_EQ(strncmp(whitelist + entries[2].offset, "net.ip", entries[2].length), 0);
  ASSERT_EQ(strncmp(whitelist + entries[1].offset, "net.addr", entries[1].length), 0);
  ASSERT_EQ(strncmp(whitelist + entries[0].offset, "net.mask", entries[0].length), 0);

  filter_swl_state_t state;

  state.hash = hash;
  state.source = source;
  state.whitelist = whitelist;
  state.whitelist_size = sizeof(whitelist);
  state.whitelist_entries = entries;
  state.whitelist_entry_count = entry_count;

  char setting_composed[] = "net.ip";
  ASSERT_TRUE(setting_in_whitelist(&state, setting_composed, strlen(setting_composed)));

  char setting_composed2[] = "ssh.enable";
  ASSERT_FALSE(setting_in_whitelist(&state, setting_composed2, strlen(setting_composed2)));

  cmph_destroy(hash);
  cmph_io_vector_adapter_destroy(source);

  free(entries);
}

TEST_F(ZmqAdapterTest, Error_LeadingOrTrailingWhitespace) {

  char whitelist[] = 
    "  net.ip\n"
    "net.addr\n"
    "net.mask\n";

  ssize_t entries = collate_whitelist(whitelist, sizeof(whitelist) - 1, NULL, NULL);
  ASSERT_EQ(entries, -2);

  char whitelist2[] = 
    "net.ip\n"
    "net.addr   \n"
    "net.mask\n";

  entries = collate_whitelist(whitelist2, sizeof(whitelist2) - 1, NULL, NULL);
  ASSERT_EQ(entries, -3);
}

TEST_F(ZmqAdapterTest, Error_EmptyLine) {

  char whitelist[] = 
    "net.ip\n"
    "\n"
    "net.mask\n";

  ssize_t entries = collate_whitelist(whitelist, sizeof(whitelist) - 1, NULL, NULL);
  ASSERT_EQ(entries, -1);

  char whitelist2[] = 
    "net.ip\n"
    "     \t\n"
    "net.mask\n";

  entries = collate_whitelist(whitelist2, sizeof(whitelist2) - 1, NULL, NULL);
  ASSERT_EQ(entries, -1);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
