/*
 * Copyright (C) 2014 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <sys/types.h>
#include <unistd.h>

#include <libpiksi/logging.h>
#include <libsbp/file_io.h>

#include <pthread.h>
#include <aio.h>

#include "sbp_fileio.h"

#define SBP_FRAMING_MAX_PAYLOAD_SIZE 255

static void read_cb(u16 sender_id, u8 len, u8 msg[], void* context);
static void read_dir_cb(u16 sender_id, u8 len, u8 msg[], void* context);
static void remove_cb(u16 sender_id, u8 len, u8 msg[], void* context);
static void write_cb(u16 sender_id, u8 len, u8 msg[], void* context);

/* Asynchronous IO cleanup thread */
static pthread_mutex_t aio_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct aio_request_node {
  struct aiocb cb;
  struct aio_request_node *next;
} *aio_request_head;

void *aio_cleanup_thread(void *data)
{
  for (;;) {
    sleep(5);
    pthread_mutex_lock(&aio_list_mutex);
    while (aio_request_head) {
      /* Bail on first request that isn't complete */
      if (aio_error(&aio_request_head->cb) == EINPROGRESS)
        break;

      /* Close file and free resources on completed request */
      struct aio_request_node *c = aio_request_head;
      aio_request_head = c->next;
      fprintf(stderr, "%15d aio cleanup %p %d\n", time(NULL), c, aio_return(&c->cb));
      close(c->cb.aio_fildes);
      free((void*)c->cb.aio_buf);
      free(c);

    }
    pthread_mutex_unlock(&aio_list_mutex);
  }
  return NULL;
}

/** Setup file IO
 * Registers relevant SBP callbacks for file IO operations.
 */
void sbp_fileio_setup(sbp_zmq_rx_ctx_t *rx_ctx, sbp_zmq_tx_ctx_t *tx_ctx)
{
  /* Start thread to clean up async IO requests */
  pthread_t thread;
  pthread_create(&thread, NULL, aio_cleanup_thread, NULL);

  sbp_zmq_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_REQ,
                               read_cb, tx_ctx, NULL);
  sbp_zmq_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_READ_DIR_REQ,
                               read_dir_cb, tx_ctx, NULL);
  sbp_zmq_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_REMOVE,
                               remove_cb, tx_ctx, NULL);
  sbp_zmq_rx_callback_register(rx_ctx, SBP_MSG_FILEIO_WRITE_REQ,
                               write_cb, tx_ctx, NULL);
}

/** File read callback.
 * Responds to a SBP_MSG_FILEIO_READ_REQ message.
 *
 * Reads a certain length (up to 255 bytes) from a given offset. Returns the
 * data in a SBP_MSG_FILEIO_READ_RESP message where the message length field
 * indicates how many bytes were succesfully read.
 */
static void read_cb(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  msg_fileio_read_req_t *msg = (msg_fileio_read_req_t *)msg_;
  sbp_zmq_tx_ctx_t *tx_ctx = (sbp_zmq_tx_ctx_t *)context;

  if ((len <= sizeof(*msg)) || (len == SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio read message!");
    return;
  }

  /* Add a null termination to filename */
  msg_[len] = 0;

  msg_fileio_read_resp_t *reply;
  int readlen = MIN(msg->chunk_size, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*reply));
  reply = alloca(sizeof(msg_fileio_read_resp_t) + readlen);
  reply->sequence = msg->sequence;
  int f = open(msg->filename, O_RDONLY);
  lseek(f, msg->offset, SEEK_SET);
  readlen = read(f, &reply->contents, readlen);
  if (readlen < 0)
    readlen = 0;
  close(f);

  sbp_zmq_tx_send(tx_ctx, SBP_MSG_FILEIO_READ_RESP,
                  sizeof(*reply) + readlen, (u8*)reply);
}

/** Directory listing callback.
 * Responds to a SBP_MSG_FILEIO_READ_DIR_REQ message.
 *
 * The offset parameter can be used to skip the first n elements of the file
 * list.
 *
 * Returns a SBP_MSG_FILEIO_READ_DIR_RESP message containing the directory
 * listings as a NULL delimited list.
 */
static void read_dir_cb(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  msg_fileio_read_dir_req_t *msg = (msg_fileio_read_dir_req_t *)msg_;
  sbp_zmq_tx_ctx_t *tx_ctx = (sbp_zmq_tx_ctx_t *)context;

  if ((len <= sizeof(*msg)) || (len == SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio read dir message!");
    return;
  }

  /* Add a null termination to dirname */
  msg_[len] = 0;

  struct dirent *dirent;
  u32 offset = msg->offset;
  msg_fileio_read_dir_resp_t *reply = alloca(SBP_FRAMING_MAX_PAYLOAD_SIZE);
  reply->sequence = msg->sequence;
  DIR *dir = opendir(msg->dirname);
  while (offset && (dirent = readdir(dir)))
    offset--;

  len = 0;
  size_t max_len = SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*reply);
  while ((dirent = readdir(dir))) {
    if (strlen(dirent->d_name) > (max_len - len - 1))
      break;
    strcpy((char*)reply->contents + len, dirent->d_name);
    len += strlen(dirent->d_name) + 1;
  }

  closedir(dir);

  sbp_zmq_tx_send(tx_ctx, SBP_MSG_FILEIO_READ_DIR_RESP,
                  sizeof(*reply) + len, (u8*)reply);
}

/* Remove file callback.
 * Responds to a SBP_MSG_FILEIO_REMOVE message.
 */
static void remove_cb(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)context;
  (void)sender_id;

  if ((len < 1) || (len == SBP_FRAMING_MAX_PAYLOAD_SIZE)) {
    piksi_log(LOG_WARNING, "Invalid fileio remove message!");
    return;
  }

  /* Add a null termination to filename */
  msg[len] = 0;

  unlink((char*)msg);
}

/* Write to file callback.
 * Responds to a SBP_MSG_FILEIO_WRITE_REQ message.
 *
 * Writes a certain length (up to 255 bytes) at a given offset. Returns a copy
 * of the original SBP_MSG_FILEIO_WRITE_RESP message to check integrity of
 * the write.
 */
static void write_cb(u16 sender_id, u8 len, u8 msg_[], void *context)
{
  (void)sender_id;
  msg_fileio_write_req_t *msg = (msg_fileio_write_req_t *)msg_;
  sbp_zmq_tx_ctx_t *tx_ctx = (sbp_zmq_tx_ctx_t *)context;

  if ((len <= sizeof(*msg) + 2) ||
      (strnlen(msg->filename, SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*msg)) ==
                              SBP_FRAMING_MAX_PAYLOAD_SIZE - sizeof(*msg))) {
    piksi_log(LOG_WARNING, "Invalid fileio write message!");
    return;
  }

  u8 headerlen = sizeof(*msg) + strlen(msg->filename) + 1;
  int f = open(msg->filename, O_WRONLY | O_CREAT, 0666);
  //lseek(f, msg->offset, SEEK_SET);
  //write(f, msg_ + headerlen, len - headerlen);
  //close(f);

  /* Set up and submit async IO request */
  struct aio_request_node *c = calloc(1, sizeof(*c));
  c->cb.aio_fildes = f;
  c->cb.aio_offset = msg->offset;
  c->cb.aio_nbytes = len - headerlen;
  c->cb.aio_buf = malloc(c->cb.aio_nbytes);
  memcpy((void*)c->cb.aio_buf, msg_ + headerlen, c->cb.aio_nbytes);
  fprintf(stderr, "%15d aio submit %p write(%d (%s), ..., %d) @ 0x%x\n",
          time(NULL), c, f, msg->filename, c->cb.aio_nbytes, msg->offset);
  aio_write(&c->cb);

  /* Add request to list to be cleaned up when done */
  pthread_mutex_lock(&aio_list_mutex);
  if (aio_request_head == NULL) {
    aio_request_head = c;
  } else {
    struct aio_request_node *x;
    for (x = aio_request_head; x->next; x = x->next)
      ;
    x->next = c;
  }
  pthread_mutex_unlock(&aio_list_mutex);

  msg_fileio_write_resp_t reply = {.sequence = msg->sequence};
  sbp_zmq_tx_send(tx_ctx, SBP_MSG_FILEIO_WRITE_RESP,
                  sizeof(reply), (u8*)&reply);
}
