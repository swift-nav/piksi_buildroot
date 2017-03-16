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

#include <curl/curl.h>
#include <stdlib.h>
#include <unistd.h>

#include <libsbp/observation.h>
#include <sbp_zmq.h>

//
// Download Daemon - connects to Skylark and receives SBP messages.
//

// Callback used by libcurl to pass data read from Skylark. Writes to pipe.
//
size_t download_callback(void *p, size_t size, size_t n, void *up)
{
  //  printf("download_callback: size=%d n=%d\n", size, n);
  int *fd = (int *)up;
  ssize_t m = write(*fd, p, size * n);
  //  printf("download_callback_WRITE: m=%d\n", m);
  return m;
}

// Download process. Calls libcurl to connect to Skylark. Takes a pipe fd.
//
void download(int fd)
{
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    exit(EXIT_FAILURE);
  }
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    curl_global_cleanup();
    exit(EXIT_FAILURE);
  }
  curl_easy_setopt(curl, CURLOPT_URL, "https://broker.skylark2.swiftnav.com");
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &download_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fd);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk,
                            "Accept: application/vnd.swiftnav.broker.v1+sbp2");
  chunk = curl_slist_append(chunk,
                            "Device-Uid: 22222222-2222-2222-2222-222222222222");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    exit(EXIT_FAILURE);
  }
  curl_easy_cleanup(curl);
  curl_global_cleanup();
}

// Test sink process. Takes a pipe fd and reads from it to STDOUT.
//
void sink(int fd)
{
  char buf[1024];
  for (;;) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    ssize_t m = fwrite(buf, 1, n, stdout);
    if (m < n) {
      break;
    }
    fflush(stdout);
  }
}

// Msg reading callback for sbp_process. Reads from pipe.
//
u32 msg_read(u8 *buf, u32 n, void *context)
{
  // printf("msg_read: n=%d\n", n);
  int *fd = (int *)context;
  ssize_t m = read(*fd, buf, n);
  // printf("msg_read_READ: m=%d\n", m);
  return m;
}

// SBP msg callback - sends message to SBP zmq.
//
void msg_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  // TODO this should be made more generic so that the same callback can service
  //      multiple message types - e.g., the context should be a struct of both
  //      the sbp_zmq_state_t and the msg type.
  printf("obs msg_callback\n");
  sbp_zmq_state_t *sbp_zmq_state = (sbp_zmq_state_t *)context;
  // TODO is this right? Can I just pass through the msg and len like this?
  //      I think this is ok - with the exception of fixing the SENDER_ID
  //      not sure the best way to fix sender id.
  sbp_zmq_message_send(sbp_zmq_state, SBP_MSG_OBS, len, msg);
}

void base_msg_llh_callback(u16 sender_id, u8 len, u8 msg[],
                                  void *context)
{
  printf("base llh msg_callback\n");
  sbp_zmq_state_t *sbp_zmq_state = (sbp_zmq_state_t *)context;
  sbp_zmq_message_send(sbp_zmq_state, SBP_MSG_BASE_POS_LLH, len, msg);
}

void base_msg_ecef_callback(u16 sender_id, u8 len, u8 msg[],
                                   void *context)
{
  printf("base ecef msg_callback\n");
  sbp_zmq_state_t *sbp_zmq_state = (sbp_zmq_state_t *)context;
  sbp_zmq_message_send(sbp_zmq_state, SBP_MSG_BASE_POS_ECEF, len, msg);
}

// Read loop process. Takes a pipe fd and reads from it to send to SBP zmq.
//
void msg_loop(int fd)
{
  // TODO sender id is not going to be right here, since we're relaying
  // messages.
  sbp_zmq_config_t sbp_zmq_config = {.sbp_sender_id = SBP_SENDER_ID,
                                     .pub_endpoint = ">tcp://localhost:43061",
                                     .sub_endpoint = ">tcp://localhost:43060"};
  // SBP zmq state to use for sending messages from Skylark.
  sbp_zmq_state_t sbp_zmq_state;
  if (sbp_zmq_init(&sbp_zmq_state, &sbp_zmq_config) < 0) {
    exit(EXIT_FAILURE);
  }
  // SBP state used for building messages out of data from Skylark.
  sbp_state_t sbp_state;
  sbp_msg_callbacks_node_t callback_node;
  sbp_state_init(&sbp_state);
  sbp_state_set_io_context(&sbp_state, &fd);
  // TODO moar messages to register
  sbp_register_callback(&sbp_state, SBP_MSG_OBS, &msg_callback, &sbp_zmq_state,
                        &callback_node);
  sbp_msg_callbacks_node_t base_ecef_callback_node;
  sbp_register_callback(&sbp_state, SBP_MSG_BASE_POS_ECEF,
                        &base_msg_ecef_callback, &sbp_zmq_state,
                        &base_ecef_callback_node);
  sbp_msg_callbacks_node_t base_llh_callback_node;
  sbp_register_callback(&sbp_state, SBP_MSG_BASE_POS_LLH,
                        &base_msg_llh_callback, &sbp_zmq_state,
                        &base_llh_callback_node);
  // SBP state processing loop - continuously reads from the pipe and builds
  // messages to send to SBP zmq.
  for (;;) {
    sbp_process(&sbp_state, &msg_read);
  }
  sbp_zmq_deinit(&sbp_zmq_state);
}

// Main entry point - sets up a pipe and forks off a download process and a read
// loop forwarder process.
//
int main(void)
{
  printf("starting download daemon\n");
  int fds[2], ret = pipe(fds);
  if (ret < 0) {
    exit(EXIT_FAILURE);
  }
  ret = fork();
  if (ret < 0) {
    exit(EXIT_FAILURE);
  } else if (ret == 0) {
    close(fds[0]);
    download(fds[1]);
  } else {
    close(fds[1]);
    msg_loop(fds[0]);
    waitpid(ret, &ret, 0);
  }
  exit(EXIT_SUCCESS);
}

// Test entry point - sets up a pipe and forks off a download process and a read
// loop that prints to STDOUT.
//
int test(void)
{
  int fds[2], ret = pipe(fds);
  if (ret < 0) {
    exit(EXIT_FAILURE);
  }
  ret = fork();
  if (ret < 0) {
    exit(EXIT_FAILURE);
  } else if (ret == 0) {
    close(fds[0]);
    download(fds[1]);
  } else {
    close(fds[1]);
    sink(fds[0]);
    waitpid(ret, &ret, 0);
  }
  exit(EXIT_SUCCESS);
}
