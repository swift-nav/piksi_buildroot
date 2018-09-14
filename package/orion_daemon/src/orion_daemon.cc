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

class SbpCtx {
public:
  bool setup() {
    loop_ = pk_loop_create();
    if (loop_ == nullptr) {
      return false;
    }

    pubsub_ = sbp_pubsub_create(PUB_ENDPOINT, SUB_ENDPOINT);
    if (pubsub_ == nullptr) {
      return false;
    }

    if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(pubsub_), loop_) != 0) {
      return false;
    }

    return true;
  }

  void teardown() {
    if (loop_ != nullptr) {
      pk_loop_destroy(&loop_);
    }

    if (pubsub_ != nullptr) {
      sbp_pubsub_destroy(&pubsub_);
    }
  }

  void recv(uint16_t type, sbp_msg_callback_t callback, void *context) {
    sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(pubsub_), type, callback, context, nullptr);
    pk_loop_run_simple(loop_);
  }

  void send(const orion_proto::SbpFrame &sbp_frame) {
    sbp_tx_send(sbp_pubsub_tx_ctx_get(pubsub_), sbp_frame.type(), sbp_frame.length(), const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(sbp_frame.payload().data())));
  }

private:
  pk_loop_t *loop_{nullptr};
  sbp_pubsub_ctx_t *pubsub_{nullptr};
};

static void pos_llh_callback(uint16_t sender, uint8_t length, uint8_t *payload, void *context) {
  auto streamer = static_cast<grpc::ClientReaderWriter<orion_proto::SbpFrame, orion_proto::SbpFrame> *>(context);
  orion_proto::SbpFrame sbp_frame;
  sbp_frame.set_type(SBP_MSG_POS_LLH);
  sbp_frame.set_sender(sender);
  sbp_frame.set_length(length);
  sbp_frame.set_payload(payload, length);
  streamer->Write(sbp_frame);
}

static void writer(grpc::ClientReaderWriter<orion_proto::SbpFrame, orion_proto::SbpFrame> *streamer, SbpCtx *sbp_ctx) {
  sbp_ctx->recv(SBP_MSG_POS_LLH, pos_llh_callback, streamer);
}

static void run() {
  SbpCtx sbp_ctx;
  if (sbp_ctx.setup()) {
    auto chan = grpc::CreateChannel(CORRECTION_GENERATOR_PORT, grpc::InsecureChannelCredentials());
    auto stub(orion_proto::CorrectionGenerator::NewStub(chan));
    grpc::ClientContext context;
    auto streamer(stub->stream_input_output(&context));

    auto thread = std::thread(&writer, streamer.get(), &sbp_ctx);

    orion_proto::SbpFrame sbp_frame;
    while (streamer->Read(&sbp_frame)) {
      sbp_ctx.send(sbp_frame);
    }

    thread.join();
    streamer->Finish();
  }
  sbp_ctx.teardown();
}

int main(int argc, char *argv[]) {
  while (true) {
    run();
  }
}
