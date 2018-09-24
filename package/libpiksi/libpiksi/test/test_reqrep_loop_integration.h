
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

#ifndef LIBPIKSI_TESTS_REQREP_LOOP_H
#define LIBPIKSI_TESTS_REQREP_LOOP_H

#include <libpiksi/loop.h>
#include <libpiksi/endpoint.h>

class ReqrepLoopIntegrationTests : public ::testing::Test {
 protected:
  virtual void SetUp()
  {
    loop = nullptr;
    req_ept = nullptr;
    rep_ept = nullptr;
  }
  virtual void TearDown()
  {
    pk_endpoint_destroy(&req_ept);
    pk_endpoint_destroy(&rep_ept);
    pk_loop_destroy(&loop);
  }

  pk_loop_t *loop;
  pk_endpoint_t *req_ept;
  pk_endpoint_t *rep_ept;
};

#endif /* LIBPIKSI_TESTS_REQREP_LOOP_H */
