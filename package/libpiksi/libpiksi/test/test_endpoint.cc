/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Dev <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <gtest/gtest.h>

#include <libpiksi_tests.h>

#include <libpiksi/endpoint.h>

extern "C" bool pk_endpoint_test(void);

TEST_F(LibpiksiTests, endpointTests) {
  pk_endpoint_t *ept = nullptr;

  /* create with invalid inputs */
  {
    ept = pk_endpoint_create("blahbloofoo", PK_ENDPOINT_PUB);
    ASSERT_EQ(ept, nullptr);

    ept = pk_endpoint_create("tcp://127.0.0.1:49010", (pk_endpoint_type)-1);
    ASSERT_EQ(ept, nullptr);
  }

  /* create server pub and connect pub */
  {
    ept = pk_endpoint_create("@tcp://127.0.0.1:49010", PK_ENDPOINT_PUB);
    ASSERT_NE(ept, nullptr);
    pk_endpoint_type type = pk_endpoint_type_get(ept);
    ASSERT_EQ(type, PK_ENDPOINT_PUB);
    pk_endpoint_destroy(&ept);
    ASSERT_EQ(ept, nullptr);

    ept = pk_endpoint_create(">tcp://127.0.0.1:49010", PK_ENDPOINT_PUB);
    ASSERT_NE(ept, nullptr);
    pk_endpoint_destroy(&ept);
    ASSERT_EQ(ept, nullptr);
  }

  /* create server sub and connect sub */
  {
    ept = pk_endpoint_create("@tcp://127.0.0.1:49010", PK_ENDPOINT_SUB);
    ASSERT_NE(ept, nullptr);
    pk_endpoint_type type = pk_endpoint_type_get(ept);
    ASSERT_EQ(type, PK_ENDPOINT_SUB);
    pk_endpoint_destroy(&ept);
    ASSERT_EQ(ept, nullptr);

    ept = pk_endpoint_create(">tcp://127.0.0.1:49010", PK_ENDPOINT_SUB);
    ASSERT_NE(ept, nullptr);
    int fd = pk_endpoint_poll_handle_get(ept);
    ASSERT_NE(fd, -1);
    pk_endpoint_destroy(&ept);
    ASSERT_EQ(ept, nullptr);
  }

}

