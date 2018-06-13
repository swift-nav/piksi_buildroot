
/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef LIBPIKSI_TESTS_PUBSUB_LOOP_H
#define LIBPIKSI_TESTS_PUBSUB_LOOP_H

#include <libpiksi/loop.h>
#include <libpiksi/endpoint.h>

class PubsubLoopIntegrationTests : public ::testing::Test {
  protected:
    virtual void SetUp() {
      loop = nullptr;
      sub_ept = nullptr;
      pub_ept = nullptr;
    }
    virtual void TearDown() {
      pk_endpoint_destroy(&sub_ept);
      pk_endpoint_destroy(&pub_ept);
      pk_loop_destroy(&loop);
    }

  pk_loop_t *loop;
  pk_endpoint_t *sub_ept;
  pk_endpoint_t *pub_ept;
};

#endif /* LIBPIKSI_TESTS_PUBSUB_LOOP_H */
