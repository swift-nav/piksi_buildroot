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


#include "orion_client.grpc.pb.h"
#include <grpcpp/create_channel.h>

void run_client(const std::string &port) {
  auto chan = grpc::CreateChannel(port, grpc::InsecureChannelCredentials());
  auto stub(orion_client_proto::Corrections::NewStub(chan));
  grpc::ClientContext context;
  auto streamer(stub->stream_input_output(&context));
  orion_client_proto::SbpFrame input;
  orion_client_proto::SbpFrame output;
  printf("XXXXXX start\n");
  streamer->Write(output);
  while (streamer->Read(&input)) {
    printf("XXXXXX got something\n");
    streamer->Write(output);
  }
  printf("XXXXXX stop\n");
  streamer->Finish();
}

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  run_client("correction-generator-ser-QLOUI52-1037726248.us-west-2.elb.amazonaws.com:9000");
  return 0;
}
