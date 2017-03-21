/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Mark Fine <mark@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdlib.h>
#include <unistd.h>

#include <libskylark.h>
#include <sbp_zmq.h>

//
// Download Daemon - connects to Skylark and receives SBP messages.
//

// TODO (mookerji): Command line stuff?

// Main entry point - sets up a pipe and forks off a download process and a read
// loop forwarder process.
//
int main(void)
{
  log_info("starting download daemon\n");
  int fds[2], ret = pipe(fds);
  if (ret < 0) {
    exit(EXIT_FAILURE);
  }
  // SBP ZMQ stuff
  // TODO (mookerji): Parse from the command line or something?
  sbp_zmq_config_t sbp_zmq_config = {
    .sbp_sender_id = SBP_SENDER_ID,
    .pub_endpoint = ">tcp://localhost:43061",
    .sub_endpoint = ">tcp://localhost:43060"
  };
  log_info("Getting config\n");
  // libskylark config
  skylark_client_config_t config;
  (void)get_config(&config);
  log_info("Got config\n");
  config.fd_in = fds[0];
  config.fd_out = fds[1];
  config.enabled = true;
  memcpy(&config.sbp_zmq_config,
         &sbp_zmq_config, sizeof(sbp_zmq_config_t));
  log_client_config(&config);
  // Fork!
  ret = fork();
  if (ret < 0) {
    exit(EXIT_FAILURE);
  } else if (ret == 0) {
    close(config.fd_in);
    download_process(&config);
  } else {
    close(config.fd_out);
    download_io_loop(&config);
    waitpid(ret, &ret, 0);
  }
  exit(EXIT_SUCCESS);
}
