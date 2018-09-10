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


#include "orion.grpc.pb.h"
#include <grpcpp/create_channel.h>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/loop.h>
#include <libpiksi/logging.h>
#include <libsbp/navigation.h>
#include <libsbp/sbp.h>

static const char *CORRECTION_GENERATOR_PORT = "correction-generator-ser-QLOUI52-1037726248.us-west-2.elb.amazonaws.com:9000";

static const char *PUB_ENDPOINT = "ipc:///var/run/sockets/skylark.sub";

static const char *SUB_ENDPOINT = "ipc:///var/run/sockets/skylark.pub";

struct Ctx {
  pk_loop_t *loop{nullptr};
  sbp_pubsub_ctx_t *pubsub_ctx{nullptr};

  bool setup() {
    loop = pk_loop_create();
    if (loop == nullptr) {
      return false;
    }

    pubsub_ctx = sbp_pubsub_create(PUB_ENDPOINT, SUB_ENDPOINT);
    if (pubsub_ctx == nullptr) {
      return false;
    }

    if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(pubsub_ctx), loop) != 0) {
      return false;
    }

    return true;
  }

  void teardown() {
    if (loop != nullptr) {
      pk_loop_destroy(&loop);
    }

    if (pubsub_ctx != nullptr) {
      sbp_pubsub_destroy(&pubsub_ctx);
    }
  }
};

void pos_llh_callback(uint16_t sender, uint8_t length, uint8_t *payload, void *context) {
  auto streamer = static_cast<grpc::ClientReaderWriter<orion_proto::SbpFrame, orion_proto::SbpFrame> *>(context);
  orion_proto::SbpFrame sbp_frame;
  sbp_frame.set_type(SBP_MSG_POS_LLH);
  sbp_frame.set_sender(sender);
  sbp_frame.set_length(length);
  sbp_frame.set_payload(payload, length);
  streamer->Write(sbp_frame);
}

static void writer(grpc::ClientReaderWriter<orion_proto::SbpFrame, orion_proto::SbpFrame> *streamer, Ctx *ctx) {
  sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(ctx->pubsub_ctx), SBP_MSG_POS_LLH, pos_llh_callback, streamer, nullptr);
  pk_loop_run_simple(ctx->loop);
}

static void run_client(const std::string &port) {
  auto chan = grpc::CreateChannel(port, grpc::InsecureChannelCredentials());
  auto stub(orion_proto::CorrectionGenerator::NewStub(chan));
  grpc::ClientContext context;
  auto streamer(stub->stream_input_output(&context));

  Ctx ctx;
  ctx.setup();
  auto thread = std::thread(&writer, streamer.get(), &ctx);

  orion_proto::SbpFrame sbp_frame;
  while (streamer->Read(&sbp_frame)) {
    sbp_tx_send(sbp_pubsub_tx_ctx_get(ctx.pubsub_ctx), sbp_frame.type(), sbp_frame.length(), const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(sbp_frame.payload().data())));
  }
  streamer->Finish();

  thread.join();
  ctx.teardown();
}

int main(int argc, char *argv[])
{
  if (argc == 2 && argv[2] != "--settings") {
    return 0;
  }

  run_client(argc == 2 ? argv[1] : CORRECTION_GENERATOR_PORT);

  return 0;
}
