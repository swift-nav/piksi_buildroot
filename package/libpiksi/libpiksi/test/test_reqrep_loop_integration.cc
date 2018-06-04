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

#include <libpiksi/loop.h>
#include <libpiksi/endpoint.h>

struct reqrep_ctx_s {
  pk_endpoint_t *req_ept;
  pk_endpoint_t *rep_ept;
  int sent;
  int recvd;
  int last_req;
};

static void test_req_cb(pk_loop_t *loop, void *handle, void *context)
{
  (void)handle;
  struct reqrep_ctx_s *ctx = (struct reqrep_ctx_s *)context;

  u8 req_value = 0;
  int result = pk_endpoint_read(ctx->req_ept, &req_value, sizeof(req_value));
  EXPECT_EQ(result, 1);
  EXPECT_EQ(req_value, ctx->last_req);
  if (req_value == ctx->last_req) ctx->recvd++;
  if (ctx->recvd > 1) {
    pk_loop_stop(loop);
  }
}

static void test_rep_cb(pk_loop_t *loop, void *handle, void *context)
{
  (void)handle;
  struct reqrep_ctx_s *ctx = (struct reqrep_ctx_s *)context;

  u8 rep_value = 0;
  int result = pk_endpoint_read(ctx->rep_ept, &rep_value, sizeof(rep_value));
  EXPECT_EQ(result, 1);
  result = pk_endpoint_send(ctx->rep_ept, &rep_value, sizeof(rep_value));
  EXPECT_EQ(result, 0);
}

static void test_timeout_cb(pk_loop_t *loop, void *handle, void *context)
{
  (void)loop;
  (void)handle;
  struct reqrep_ctx_s *ctx = (struct reqrep_ctx_s *)context;
  if (ctx->sent == ctx->recvd) {
    u8 simple_message = ctx->sent + 1;
    int result = pk_endpoint_send(ctx->req_ept, &simple_message, sizeof(simple_message));
    EXPECT_EQ(result, 0);
    if (result == 0) {
      ctx->last_req = simple_message;
      ctx->sent++;
    }
  }
}

TEST_F(LibpiksiTests, reqrepLoopIntegrationTest)
{
  pk_loop_t *loop = pk_loop_create();
  ASSERT_NE(loop, nullptr);

  pk_endpoint_t *rep_ept = pk_endpoint_create("tcp://127.0.0.1:49010", PK_ENDPOINT_REP);
  ASSERT_NE(rep_ept, nullptr);

  pk_endpoint_t *req_ept = pk_endpoint_create("tcp://127.0.0.1:49010", PK_ENDPOINT_REQ);
  ASSERT_NE(req_ept, nullptr);

  struct reqrep_ctx_s ctx = { .req_ept = req_ept,
                              .rep_ept = rep_ept,
                              .sent = 0,
                              .recvd = 0,
                              .last_req = 0 };
  ASSERT_NE(pk_loop_endpoint_reader_add(loop, ctx.req_ept, test_req_cb, &ctx), nullptr);
  ASSERT_NE(pk_loop_endpoint_reader_add(loop, ctx.rep_ept, test_rep_cb, &ctx), nullptr);
  ASSERT_NE(pk_loop_timer_add(loop, 100, test_timeout_cb, &ctx), nullptr);

  pk_loop_run_simple_with_timeout(loop, 2000);

  EXPECT_GT(ctx.recvd, 0);
  EXPECT_EQ(ctx.sent, ctx.recvd);
  pk_endpoint_destroy(&rep_ept);
  EXPECT_EQ(rep_ept, nullptr);
  pk_endpoint_destroy(&req_ept);
  EXPECT_EQ(req_ept, nullptr);
  pk_loop_destroy(&loop);
  EXPECT_EQ(loop, nullptr);
}

