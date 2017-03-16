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

#include "gtest/gtest.h"

extern "C" {
#include "libskylark.h"
}

#define DEFAULT_DEVICE_UID      "22222222-2222-2222-2222-222222222222"

namespace {

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

  TEST(configuration, load_broker_endpoint)
  {
    char broker_endpoint[BUFSIZE];
    RC rc = get_broker_endpoint(broker_endpoint);
    ASSERT_EQ(rc, NO_ERROR);
    ASSERT_STREQ(broker_endpoint, DEFAULT_BROKER_ENDPOINT);
  }

  TEST(configuration, load_configuration)
  {
    client_config_t config;
    RC rc = init_config(&config);
    ASSERT_EQ(rc, NO_ERROR);
    ASSERT_STREQ(config.endpoint_url, DEFAULT_BROKER_ENDPOINT);
    ASSERT_STREQ(config.accept_type_header, SBP_V2_ACCEPT_TYPE);
    ASSERT_STREQ(config.content_type_header, SBP_V2_CONTENT_TYPE);
    ASSERT_STREQ(config.encoding, STREAM_ENCODING);
    ASSERT_STREQ(config.device_uuid, DEFAULT_DEVICE_UID);
    ASSERT_STREQ(config.device_header,
                 "Device-Uid: 22222222-2222-2222-2222-222222222222");
    ASSERT_EQ(config.sbp_sender_id, 222);
  }

  TEST(skylark_connection, download_process)
  {
    client_config_t config;
    RC rc = init_config(&config);
    ASSERT_EQ(rc, NO_ERROR);

  }

  TEST(skylark_connection, upload_process)
  {
    client_config_t config;
    RC rc = init_config(&config);
    ASSERT_EQ(rc, NO_ERROR);
  }
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
