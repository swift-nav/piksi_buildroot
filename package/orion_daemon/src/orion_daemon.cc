/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <condition_variable>
#include <mutex>
#include <cassert>
#include <cstdlib>
#include <thread>
#include <unistd.h>
#include <libpiksi/logging.h>
#include <libpiksi/loop.h>
#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/settings.h>
#include <libsbp/navigation.h>
#include <libsbp/sbp.h>
#include <grpcpp/create_channel.h>
#include "orion.grpc.pb.h"

static const char *PROGRAM_NAME = "orion_daemon";

static const char *PUB_ENDPOINT = "ipc:///var/run/sockets/skylark.sub";

static const char *SUB_ENDPOINT = "ipc:///var/run/sockets/skylark.pub";

static char port[256] = "";

static bool enable = false;

static bool enabled = false;

static bool changed = false;

static std::condition_variable condition;

static std::mutex mutex;

static uint32_t then;

static bool active()
{
  std::unique_lock<std::mutex> lock{mutex};
  condition.wait(lock, [] { return enabled; });
  changed = false;
  return true;
}

static bool inactive()
{
  std::unique_lock<std::mutex> lock{mutex};
  return changed || !enabled;
}

static int settings_callback(void *context)
{
  assert(context == nullptr);
  std::unique_lock<std::mutex> lock{mutex};
  enabled = enable && strlen(port) != 0;
  changed = true;
  condition.notify_all();
  return SBP_SETTINGS_WRITE_STATUS_OK;
}

static void pos_llh_callback(uint16_t sender, uint8_t length, uint8_t *payload, void *context)
{
  auto const msg_pos_llh = reinterpret_cast<const msg_pos_llh_t *>(payload);
  if ((msg_pos_llh->flags & 0x7) == 0) {
    return;
  }
  const uint32_t now = msg_pos_llh->tow / 1000;
  if (now == then) {
    return;
  }
  then = now;
  assert(context != nullptr);
  auto streamer =
    static_cast<grpc::ClientReaderWriter<orion_proto::SbpFrame, orion_proto::SbpFrame> *>(context);
  orion_proto::SbpFrame sbp_frame;
  sbp_frame.set_type(SBP_MSG_POS_LLH);
  sbp_frame.set_sender(sender);
  sbp_frame.set_length(length);
  sbp_frame.set_payload(payload, length);
  streamer->Write(sbp_frame);
}

class Ctx {
 public:
  Ctx() : loop_(pk_loop_create())
  {
    assert(loop_ != nullptr);
  }

  ~Ctx()
  {
    pk_loop_destroy(&loop_);
  }

  bool run()
  {
    return pk_loop_run_simple(loop_) == 0;
  }

  void stop()
  {
    pk_loop_stop(loop_);
  }

 protected:
  pk_loop_t *loop_;
};

class SettingsCtx : public Ctx {
 public:
  SettingsCtx() : settings_(settings_create())
  {
    assert(settings_ != nullptr);
  }

  ~SettingsCtx()
  {
    settings_destroy(&settings_);
  }

  bool setup()
  {
    return settings_attach(settings_, loop_) == 0
           && settings_add_watch(settings_,
                                 "orion",
                                 "enable",
                                 &enable,
                                 sizeof(enable),
                                 SETTINGS_TYPE_BOOL,
                                 settings_callback,
                                 nullptr)
                == 0
           && settings_register(settings_,
                                "orion",
                                "port",
                                &port,
                                sizeof(port),
                                SETTINGS_TYPE_STRING,
                                settings_callback,
                                nullptr)
                == 0;
  }

 private:
  settings_ctx_t *settings_;
};

class SbpCtx : public Ctx {
 public:
  SbpCtx() : pubsub_(sbp_pubsub_create(PUB_ENDPOINT, SUB_ENDPOINT))
  {
    assert(pubsub_ != nullptr);
  }

  ~SbpCtx()
  {
    sbp_pubsub_destroy(&pubsub_);
  }

  bool setup(void *context)
  {
    return sbp_rx_attach(sbp_pubsub_rx_ctx_get(pubsub_), loop_) == 0
           && sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(pubsub_),
                                       SBP_MSG_POS_LLH,
                                       pos_llh_callback,
                                       context,
                                       nullptr)
                == 0;
  }

  bool send(const orion_proto::SbpFrame &sbp_frame)
  {
    return sbp_tx_send(sbp_pubsub_tx_ctx_get(pubsub_),
                       sbp_frame.type(),
                       sbp_frame.length(),
                       const_cast<uint8_t *>(
                         reinterpret_cast<const uint8_t *>(sbp_frame.payload().data())))
           == 0;
  }

 private:
  sbp_pubsub_ctx_t *pubsub_;
};

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  piksi_log(LOG_INFO | LOG_SBP, "Daemon started");

  SettingsCtx settings_ctx;
  if (settings_ctx.setup()) {
    piksi_log(LOG_INFO | LOG_SBP, "Settings setup");

    auto settings_thread = std::thread(&SettingsCtx::run, &settings_ctx);

    while (active()) {
      piksi_log(LOG_INFO | LOG_SBP, "Daemon active %s", port);

      auto chan = grpc::CreateChannel(port, grpc::InsecureChannelCredentials());
      auto stub(orion_proto::CorrectionGenerator::NewStub(chan));
      grpc::ClientContext context;
      auto streamer(stub->stream_input_output(&context));

      SbpCtx sbp_ctx;
      if (sbp_ctx.setup(streamer.get())) {
        piksi_log(LOG_INFO | LOG_SBP, "Sbp setup");

        auto sbp_thread = std::thread(&SbpCtx::run, &sbp_ctx);

        uint8_t uuid[16];

        std::string version;

        orion_proto::SbpFrame sbp_frame;
        while (!inactive() && streamer->Read(&sbp_frame)) {
          sbp_ctx.send(sbp_frame);

          auto data =
            reinterpret_cast<const uint8_t *>(sbp_frame.header().service_uid().payload().data());

          if (std::memcmp(uuid, data, sizeof(uuid)) != 0) {
            std::memcpy(uuid, data, sizeof(uuid));

            piksi_log(
              LOG_INFO | LOG_SBP,
              "Service UUID %02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
              uuid[0],
              uuid[1],
              uuid[2],
              uuid[3],
              uuid[4],
              uuid[5],
              uuid[6],
              uuid[7],
              uuid[8],
              uuid[9],
              uuid[10],
              uuid[11],
              uuid[12],
              uuid[13],
              uuid[14],
              uuid[15]);
          }

          if (version != sbp_frame.header().version()) {
            version = sbp_frame.header().version();

            piksi_log(LOG_INFO | LOG_SBP, "Version %s", version.c_str());
          }
        }

        piksi_log(LOG_INFO | LOG_SBP, "Daemon inactive");

        sbp_ctx.stop();
        sbp_thread.join();
      }
    }

    assert(false);
    settings_thread.join();
  }

  exit(EXIT_SUCCESS);
}
