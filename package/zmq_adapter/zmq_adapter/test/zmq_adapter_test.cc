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

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include "filter.h"
#include "logging.h"
#include "protocols.h"

#include "filter_settings_whitelist.h"
#include "filter_settings_whitelist.c"

char* settings_whitelist_dir;

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

#define SBP_FRAME_SIZE_MAX 264
#define SBP_PAYLOAD_SIZE_MAX (255u)

static u8 send_buffer[SBP_FRAME_SIZE_MAX];
static u32 send_buffer_length;

static u32 send_buffer_write(u8 *buff, u32 n, void *context)
{
  u32 len = MIN(sizeof(send_buffer) - send_buffer_length, n);
  memcpy(&send_buffer[send_buffer_length], buff, len);
  send_buffer_length += len;
  return len;
}

int build_sbp_message(const char* section, const char* name, const char* value)
{
  u8 msg[SBP_PAYLOAD_SIZE_MAX];
  u8 msg_n = 0;
  int written;

  /* Section */
  written =
    snprintf((char *)&msg[msg_n], SBP_PAYLOAD_SIZE_MAX - msg_n, "%s", section);
  if ((written < 0) || ((u8)written >= SBP_PAYLOAD_SIZE_MAX - msg_n)) {
    return -1;
  }
  msg_n = (u8)(msg_n + written + 1);

  /* Name */
  written =
    snprintf((char *)&msg[msg_n], SBP_PAYLOAD_SIZE_MAX - msg_n, "%s", name);
  if ((written < 0) || ((u8)written >= SBP_PAYLOAD_SIZE_MAX - msg_n)) {
    return -1;
  }
  msg_n = (u8)(msg_n + written + 1);

  /* Value */
  written =
    snprintf((char *)&msg[msg_n], SBP_PAYLOAD_SIZE_MAX - msg_n, "%s", value);
  if ((written < 0) || ((u8)written >= SBP_PAYLOAD_SIZE_MAX - msg_n)) {
    return -1;
  }
  msg_n = (u8)(msg_n + written + 1);

  sbp_state_t sbp_state;
  sbp_state_init(&sbp_state);

  u16 sender_id = 0x42U;
  u16 msg_type = SBP_MSG_SETTINGS_WRITE;

  send_buffer_length = 0;

  if (sbp_send_message(&sbp_state, msg_type, sender_id, msg_n, msg,
                       send_buffer_write) != SBP_OK) {
    return -1;
  }
}

static void ProcessSbpPacket_Helper(const char* whitelist_filename,
                                    bool reject_both)
{
  char settings_whitelist_config[128];
  snprintf(settings_whitelist_config, sizeof(settings_whitelist_config),
           "%s/%s", settings_whitelist_dir, whitelist_filename);

  fprintf(stderr, "settings_whitelist_config: %s\n", settings_whitelist_config);

  filter_spec_t filter_specs[] = {
    { .name = "none", .filename = NULL },
    { .name = FILTER_SWL_NAME, .filename = settings_whitelist_config },
  };

  filter_list_t* filters = filter_create(filter_specs, COUNT_OF(filter_specs));

  ASSERT_NE(filters, nullptr);

  build_sbp_message("ssh", "enable", "1");
  int reject = filter_process(filters, send_buffer, send_buffer_length);

  ASSERT_NE(reject, 0);

  build_sbp_message("section_name", "setting_name", "1");
  reject = filter_process(filters, send_buffer, send_buffer_length);

  if (reject_both)
    ASSERT_NE(reject, 0);
  else
    ASSERT_EQ(reject, 0);

  filter_destroy(&filters);
  ASSERT_EQ(filters, nullptr);
}

TEST_F(ZmqAdapterTest, ProcessSbpPacket) {
  ProcessSbpPacket_Helper("one_entry_whitelist", false);
}

TEST_F(ZmqAdapterTest, ProcessSbpPacket_EmptyWhitelist) {
  ProcessSbpPacket_Helper("empty_whitelist", true);
}

void test_exit(int code) {
  ASSERT_TRUE(false);
}

void test_log_func(int priority, const char* message, va_list args) {

  static const char* priorities[] = {
    "emerg", "alert", "crit", "error", "warn", "notice", "info", "debug"
  };

  priority &= ~LOG_FACMASK;

  fprintf(stderr, "%s: ", priorities[priority]);
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");
}

int main(int argc, char** argv) {

  filter_exit_fn = test_exit;

  logging_init("zmq_adapter_test");

  zmq_adapter_set_log_fn(test_log_func);

  settings_whitelist_dir = getenv("SETTINGS_WHITELIST_DIR");
  assert(settings_whitelist_dir != NULL);

  protocols_import(getenv("ADAPTER_PROTOCOL_DIR"));

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
