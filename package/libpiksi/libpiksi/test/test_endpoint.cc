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

TEST_F(LibpiksiTests, endpointTests)
{
  pk_endpoint_t *ept = nullptr;
  pk_endpoint_t *ept_srv = nullptr;

  /* create with invalid inputs */
  {
    ept = pk_endpoint_create(pk_endpoint_config()
                               .endpoint("blahbloofoo")
                               .identity("blahbloofoo")
                               .type(PK_ENDPOINT_PUB)
                               .get());
    ASSERT_EQ(ept, nullptr);

    ept = pk_endpoint_create(pk_endpoint_config()
                               .endpoint("ipc:///tmp/tmp.49010")
                               .identity("tmp.49010")
                               .type((pk_endpoint_type)-1)
                               .get());
    ASSERT_EQ(ept, nullptr);
  }

  /* create server pub and connect pub */
  {
    ept_srv = pk_endpoint_create(pk_endpoint_config()
                                   .endpoint("ipc:///tmp/tmp.49010")
                                   .identity("tmp.49010.server")
                                   .type(PK_ENDPOINT_PUB_SERVER)
                                   .get());
    ASSERT_NE(ept_srv, nullptr);
    pk_endpoint_type type = pk_endpoint_type_get(ept_srv);
    ASSERT_EQ(type, PK_ENDPOINT_PUB_SERVER);

    ept = pk_endpoint_create(pk_endpoint_config()
                               .endpoint("ipc:///tmp/tmp.49010")
                               .identity("tmp.49010.pub")
                               .type(PK_ENDPOINT_PUB)
                               .get());
    ASSERT_NE(ept, nullptr);
    pk_endpoint_destroy(&ept);
    ASSERT_EQ(ept, nullptr);

    pk_endpoint_destroy(&ept_srv);
    ASSERT_EQ(ept_srv, nullptr);
  }

  /* create server sub and connect sub */
  {
    ept_srv = pk_endpoint_create(pk_endpoint_config()
                                   .endpoint("ipc:///tmp/tmp.49010")
                                   .identity("tmp.49010.sub.server")
                                   .type(PK_ENDPOINT_SUB_SERVER)
                                   .get());
    ASSERT_NE(ept_srv, nullptr);
    pk_endpoint_type type = pk_endpoint_type_get(ept_srv);
    ASSERT_EQ(type, PK_ENDPOINT_SUB_SERVER);

    ept = pk_endpoint_create(pk_endpoint_config()
                               .endpoint("ipc:///tmp/tmp.49010")
                               .identity("tmp.49010.pub")
                               .type(PK_ENDPOINT_SUB)
                               .get());
    ASSERT_NE(ept, nullptr);
    int fd = pk_endpoint_poll_handle_get(ept);
    ASSERT_NE(fd, -1);
    pk_endpoint_destroy(&ept);
    ASSERT_EQ(ept, nullptr);

    pk_endpoint_destroy(&ept_srv);
    ASSERT_EQ(ept_srv, nullptr);
  }
}
