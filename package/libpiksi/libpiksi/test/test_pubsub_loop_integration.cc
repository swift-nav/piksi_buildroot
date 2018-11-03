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

#include <test_pubsub_loop_integration.h>

struct snd_ctx_s {
  pk_endpoint_t *ept;
  int sent;
};

struct recv_ctx_s {
  pk_endpoint_t *ept;
  int recvd;
};

#define SIMPLE_RECV_MSG "I'm a message"
#define SIMPLE_RECV_SIZE (100u)
static int test_simple_recv_cb(const u8 *data, const size_t length, void *context)
{
  struct recv_ctx_s *recv_ctx = (struct recv_ctx_s *)context;
  // use expect here because ASSERT fails to compile
  EXPECT_EQ(memcmp(data, SIMPLE_RECV_MSG, length), 0);
  recv_ctx->recvd++;
  return 0;
}

static void test_timeout_cb(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)handle;
  (void)status;
  struct snd_ctx_s *snd_ctx = (struct snd_ctx_s *)context;
  const char *simple_message = SIMPLE_RECV_MSG;
  size_t msg_len = strlen(simple_message);
  // use expect here so that we exit gracefully after the timer expires
  int result = pk_endpoint_send(snd_ctx->ept, (u8 *)simple_message, msg_len);
  EXPECT_EQ(result, 0);
  if (result == 0) snd_ctx->sent++;
}

static void test_poll_cb(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)handle;
  (void)status;

  struct recv_ctx_s *recv_ctx = (struct recv_ctx_s *)context;

  // use expect here so that we exit gracefully after the timer expires
  EXPECT_EQ(pk_endpoint_receive(recv_ctx->ept, test_simple_recv_cb, recv_ctx), 0);
  if (recv_ctx->recvd > 1) {
    pk_loop_stop(loop);
  }
}
#undef SIMPLE_RECV_MSG
#undef SIMPLE_RECV_SIZE

TEST_F(PubsubLoopIntegrationTests, pubsubLoopIntegrationTest)
{
  // this is cleaned up in TearDown
  loop = pk_loop_create();
  ASSERT_NE(loop, nullptr);

  // this is cleaned up in TearDown
  sub_ept = pk_endpoint_create("ipc:///tmp/tmp.49010", PK_ENDPOINT_SUB_SERVER);
  ASSERT_NE(sub_ept, nullptr);

  pk_endpoint_loop_add(sub_ept, loop);

  // this is cleaned up in TearDown
  pub_ept = pk_endpoint_create("ipc:///tmp/tmp.49010", PK_ENDPOINT_PUB);
  ASSERT_NE(pub_ept, nullptr);

  struct snd_ctx_s snd_ctx = {.ept = pub_ept, .sent = 0};
  ASSERT_NE(pk_loop_timer_add(loop, 100, test_timeout_cb, &snd_ctx), nullptr);

  struct recv_ctx_s recv_ctx = {.ept = sub_ept, .recvd = 0};
  ASSERT_NE(pk_loop_endpoint_reader_add(loop, recv_ctx.ept, test_poll_cb, &recv_ctx), nullptr);

  pk_loop_run_simple_with_timeout(loop, 2000);

  ASSERT_GT(recv_ctx.recvd, 0);
  ASSERT_GE(snd_ctx.sent, recv_ctx.recvd);
}
