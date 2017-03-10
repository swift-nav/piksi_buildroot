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
#include <curl/curl.h>

#include <sbp_zmq.h>
#include <libsbp/navigation.h>

//
// Upload Daemon - connects to Skylark and sends SBP messages.
//


// Callback used by libcurl to pass data to write to Skylark. Reads from pipe.
//
static size_t upload_callback(void *p, size_t size, size_t n, void *up)
{
  printf("upload_callback: size=%d n=%d\n", size, n);
  int *fd = (int *)up;
  ssize_t m = read(*fd, p, size*n);
  printf("upload_callback_READ: m=%d\n", m);
  if (m < 0) {
    return CURL_READFUNC_ABORT;
  }
  return m;
}

// Upload process. Calls libcurl to connect to Skylark. Takes a pipe fd.
//
static void upload(int fd)
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
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, &upload_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &fd);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk, "Content-Type: application/vnd.swiftnav.broker.v1+sbp2");
  chunk = curl_slist_append(chunk, "Device-Uid: 22222222-2222-2222-2222-222222222222");
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

// Test source process. Takes a pipe fd and writes to it from STDIN.
//
static void source(int fd)
{
  char buf[1024];
  for (;;) {
    ssize_t n = fread(buf, 1, sizeof(buf), stdin);
    if (n <= 0) {
      break;
    }
    ssize_t m = write(fd, buf, n);
    if (m < n) {
      break;
    }
  }
}

// Msg writing callback for msg callback. Takes a pipe fd and writes a message to it.
//
static u32 msg_write(u8 *buf, u32 n, void *context)
{
  int *fd = (int *)context;
  printf("msg_write: n=%d\n", n);
  ssize_t m = write(*fd, buf, n);
  printf("msg_write_WRITE: m=%d\n", m);
  return m;
}

// SBP msg callback - writes message to pipe.
//
static void msg_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  // TODO this should be made more generic so that the same callback can service
  //      multiple message types - e.g., the context should be a struct of both
  //      the sbp_zmq_state_t and the msg type.
  int *fd = (int *)context;
  printf("msg_callback len=%d\n", len);
  // TODO I doubt this is right - won't I need to rebuild the rest of the SBP frame?
  //      ok - now I'm going to do something probably inefficient - going to setup a
  //      new sbp_state_t object here for writing and then call sbp_send_message.
  sbp_state_t sbp_state;
  sbp_state_init(&sbp_state);
  sbp_state_set_io_context(&sbp_state, fd);
  sbp_send_message(&sbp_state, SBP_MSG_POS_LLH, sender_id, len, msg, &msg_write);
}

// zmq read loop process. Takes a pipe fd and writes messages to it from listening to SBP zmq.
//
static void msg_loop(int fd)
{
  sbp_zmq_config_t sbp_zmq_config = {
    .sbp_sender_id = SBP_SENDER_ID, // TODO problem? don't think this will end up getting used.
    .pub_endpoint = ">tcp://localhost:43061",
    .sub_endpoint = ">tcp://localhost:43060"
  };

  sbp_zmq_state_t sbp_zmq_state;
  if (sbp_zmq_init(&sbp_zmq_state, &sbp_zmq_config) < 0) {
    exit(EXIT_FAILURE);
  }

  // TODO moar messages to register
  sbp_zmq_callback_register(&sbp_zmq_state, SBP_MSG_POS_LLH, &msg_callback, &fd, NULL);
  sbp_zmq_loop(&sbp_zmq_state);

  sbp_zmq_deinit(&sbp_zmq_state);
}

// Main entry point - sets up a pipe and forks off a upload process and a message reader process.
//
int main(void)
{
  int fds[2], ret = pipe(fds);
  if (ret < 0) {
    exit(EXIT_FAILURE);
  }

  ret = fork();
  if (ret < 0) {
    exit(EXIT_FAILURE);
  } else if (ret == 0) {
    close(fds[1]);
    upload(fds[0]);
  } else {
    close(fds[0]);
    msg_loop(fds[1]);
    waitpid(ret, &ret, 0);
  }

  exit(EXIT_SUCCESS);
}

// Test entry point - sets up a pipe and forks off a upload process and a message reader loop that reads from STDIN.
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
    close(fds[1]);
    upload(fds[0]);
  } else {
    close(fds[0]);
    source(fds[1]);
    waitpid(ret, &ret, 0);
  }

  exit(EXIT_SUCCESS);
}

