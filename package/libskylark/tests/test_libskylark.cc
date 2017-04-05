/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gtest/gtest.h"

extern "C" {
#include "libskylark.h"
}

#define DEFAULT_DEVICE_UID "22222222-2222-2222-2222-222222222222"

void start_download_server() {}

void start_upload_server() {}

namespace
{
struct MockedPipe {
  std::string fifo_name_;
  int fd_;

  MockedPipe(const std::string& fifo_name, bool read_only)
      : fifo_name_(fifo_name), fd_()
  {
    int code = 0;
    if (access(fifo_name_.c_str(), F_OK) != -1) {
      code = unlink(fifo_name_.c_str());
    }
    code = mkfifo(fifo_name_.c_str(), S_IRUSR | S_IWUSR);
    if (read_only) {
      fd_ = open(fifo_name_.c_str(), O_RDONLY | O_NONBLOCK);
    } else {
      fd_ = open(fifo_name_.c_str(), O_WRONLY | O_NONBLOCK);
    }
  }

  ~MockedPipe()
  {
    close(fd_);
    unlink(fifo_name_.c_str());
  }
};

TEST(configuration, load_device_uuid)
{
  char device_uuid[BUFSIZE];
  RC rc = get_device_uuid(device_uuid);
  ASSERT_EQ(rc, NO_ERROR);
  ASSERT_STREQ(device_uuid, DEFAULT_DEVICE_UID);
}

TEST(configuration, load_device_header)
{
  char device_uuid[BUFSIZE];
  RC rc = get_device_uuid(device_uuid);
  ASSERT_EQ(rc, NO_ERROR);
  ASSERT_STREQ(device_uuid, DEFAULT_DEVICE_UID);
  char device_uuid_header[BUFSIZE];
  rc = get_device_header(device_uuid, device_uuid_header);
  ASSERT_EQ(rc, NO_ERROR);
  ASSERT_STREQ(device_uuid_header,
               "Device-Uid: 22222222-2222-2222-2222-222222222222");
}

TEST(configuration, load_configuration)
{
  client_config_t config;
  RC rc = init_config(&config);
  config.fd = 3;
  strncpy(config.endpoint_url, "localhost", sizeof("localhost"));
  log_client_config(&config);
  ASSERT_EQ(rc, NO_ERROR);
  ASSERT_STREQ(config.endpoint_url, "localhost");
  ASSERT_STREQ(config.device_uuid, DEFAULT_DEVICE_UID);
  ASSERT_STREQ(config.device_header,
               "Device-Uid: 22222222-2222-2222-2222-222222222222");
}

TEST(skylark_connection, upload_process)
{
  client_config_t config;
  RC rc = init_config(&config);
  ASSERT_EQ(rc, NO_ERROR);
  rc = setup_globals();
  ASSERT_EQ(rc, NO_ERROR);
  bool verbose_logging = false;

  MockedPipe pipe("/tmp/skylark_upload_test_fifo", true);
  config.fd = pipe.fd_;
  strcpy(config.endpoint_url, "localhost:8080");

  start_upload_server();

  if ((rc = upload_process(&config, verbose_logging)) < NO_ERROR) {
    log_client_error(rc);
  }

  teardown_globals();
}
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
