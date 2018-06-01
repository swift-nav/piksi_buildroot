/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <assert.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include <libpiksi/endpoint.h>

struct pk_endpoint_s {
  pk_endpoint_type type;
  zsock_t *zsock;
};

pk_endpoint_t * pk_endpoint_create(const char *endpoint, pk_endpoint_type type)
{
  assert(endpoint != NULL);

  pk_endpoint_t *pk_ept = (pk_endpoint_t *)malloc(sizeof(pk_endpoint_t));
  if (pk_ept == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate PK endpoint");
    goto failure;
  }

  pk_ept->type = type;
  switch (pk_ept->type)
  {
  case PK_ENDPOINT_PUB:
  {
    pk_ept->zsock = zsock_new_pub(endpoint);
    if (pk_ept->zsock == NULL) {
      piksi_log(LOG_ERR, "error creating PK PUB socket");
      goto failure;
    }
  } break;
  case PK_ENDPOINT_SUB:
  {
    pk_ept->zsock = zsock_new_sub(endpoint, "");
    if (pk_ept->zsock == NULL) {
      piksi_log(LOG_ERR, "error creating PK SUB socket");
      goto failure;
    }
  } break;
  case PK_ENDPOINT_REQ:
  {
    pk_ept->zsock = zsock_new_req(endpoint);
    if (pk_ept->zsock == NULL) {
      piksi_log(LOG_ERR, "error creating PK REQ socket");
      goto failure;
    }
  } break;
  case PK_ENDPOINT_REP:
  {
    pk_ept->zsock = zsock_new_rep(endpoint);
    if (pk_ept->zsock == NULL) {
      piksi_log(LOG_ERR, "error creating PK REP socket");
      goto failure;
    }
  } break;
  default:
  {
    piksi_log(LOG_ERR, "Unsupported endpoint type");
    goto failure;
  } break;
  } // end of switch

  return pk_ept;

failure:
  pk_endpoint_destroy(&pk_ept);
  return NULL;
}

void pk_endpoint_destroy(pk_endpoint_t **pk_ept_loc)
{
  if (pk_ept_loc == NULL || *pk_ept_loc == NULL) {
    return;
  }
  pk_endpoint_t *pk_ept = *pk_ept_loc;
  if (pk_ept->zsock != NULL) zsock_destroy(&pk_ept->zsock);
  free(pk_ept);
  *pk_ept_loc = NULL;
}

pk_endpoint_type pk_endpoint_type_get(pk_endpoint_t *pk_ept)
{
  return pk_ept->type;
}

int pk_endpoint_poll_handle_get(pk_endpoint_t *pk_ept)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_PUB);

  return zsock_fd(pk_ept->zsock);
}

/**
 * @brief pk_endpoint_receive_zmsg - helper to retrieve a single message
 * @param pk_ept: pointer to the endpoint context
 * @return a newly allocated zmsg_t instance or NULL on failure
 */
static zmsg_t * pk_endpoint_receive_zmsg(pk_endpoint_t *pk_ept)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_PUB);

  zmsg_t *msg;
  while (1) {
    msg = zmsg_recv(pk_ept->zsock);
    if (msg != NULL) {
      /* Break on success */
      return msg;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in zmsg_recv()");
      return NULL;
    }
  }
}

/**
 * @brief fill_buffer_from_zmsg - helper to collapse zframes into buffer
 * @param msg: zmq message to copy into supplied buffer
 * @param buffer: pointer to memory to fill
 * @param count: size of fill buffer
 * @return the number of bytes written to the buffer, or -1 for error
 */
static ssize_t fill_buffer_from_zmsg(zmsg_t *msg, u8 *buffer, size_t count)
{
  assert(msg != NULL);
  assert(buffer != NULL);

  size_t index = 0;
  zframe_t *frame;
  bool overflow = false;
  for (frame = zmsg_first(msg); frame != NULL; frame = zmsg_next(msg)) {
    const void *data = zframe_data(frame);
    size_t size = zframe_size(frame);

    if (index + size > count) {
      overflow = true;
    }
    size_t copy_length = SWFT_MIN(size, count - index);
    if (copy_length > 0) {
      memcpy((uint8_t *)(buffer + index), data, copy_length);
      index += copy_length;
    }
    if (overflow) {
      break;
    }
  }

  if (overflow) {
    piksi_log(LOG_WARNING, "overflow in zmsg buffer fill");
  }
  return index;
}

ssize_t pk_endpoint_read(pk_endpoint_t *pk_ept, u8 *buffer, size_t count)
{
  assert(pk_ept != NULL);
  assert(buffer != NULL);

  zmsg_t * msg = pk_endpoint_receive_zmsg(pk_ept);
  if (msg == NULL) {
    return -1;
  }

  ssize_t result = fill_buffer_from_zmsg(msg, buffer, count);

  zmsg_destroy(&msg);
  assert(msg == NULL);

  return result;
}

/**
 * @brief dup_zmsg_to_buffer - helper to collapse zmsg frames
 * @note  caller must free the returned buffer if call is successful
 * @param msg: zmq message to copy
 * @param buffer_loc: double pointer to place alloc'd buffer
 * @param count_loc: pointer to place the length of the returned data
 * @return 0 on success, -1 on failure
 */
static int dup_zmsg_to_buffer(zmsg_t *msg, u8 **buffer_loc, size_t *count_loc)
{
  assert(msg != NULL);
  assert(buffer_loc != NULL);
  assert(count_loc != NULL);

  size_t buffer_size = zmsg_content_size(msg);
  u8 *buffer = (u8 *)malloc(buffer_size);
  if (buffer == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate buffer for zmsg dup");
    return -1;
  }

  ssize_t buffer_index = fill_buffer_from_zmsg(msg, buffer, buffer_size);
  if (buffer_index == -1) {
    piksi_log(LOG_ERR, "failed to copy zmsg into dup buffer");
    return -1;
  }

  if (buffer_index != buffer_size) {
    piksi_log(LOG_WARNING, "dup zmsg - data copied not equal to buffer_size");
  }
  *buffer_loc = buffer;
  *count_loc = buffer_index;
  return 0;
}

int pk_endpoint_receive(pk_endpoint_t *pk_ept, pk_endpoint_receive_cb rx_cb, void *context)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_PUB);
  assert(rx_cb != NULL);

  while (ZMQ_POLLIN & zsock_events(pk_ept->zsock)) {
    zmsg_t * msg = pk_endpoint_receive_zmsg(pk_ept);
    if (msg == NULL) {
      return -1;
    }

    if (zmsg_size(msg) != 1) {
      piksi_log(LOG_WARNING, "Piksi endpoint received non-conforming message!");
    }

    u8 *dup_buffer = NULL;
    size_t dup_length = 0;
    if (dup_zmsg_to_buffer(msg, &dup_buffer, &dup_length) == 0) {
      rx_cb(dup_buffer, dup_length, context);
      free(dup_buffer);
    }

    zmsg_destroy(&msg);
  }
  return 0;
}

int pk_endpoint_send(pk_endpoint_t *pk_ept, const u8 *data, const size_t length)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_SUB);

  zmsg_t *msg = zmsg_new();
  if (msg == NULL) {
    piksi_log(LOG_ERR, "error in zmsg_new()");
    return -1;
  }

  if (zmsg_addmem(msg, data, length) != 0) {
    piksi_log(LOG_ERR, "error in zmsg_addmem()");
    zmsg_destroy(&msg);
    return -1;
  }

  while (1) {
    int ret = zmsg_send(&msg, pk_ept->zsock);
    if (ret == 0) {
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in zmsg_send()");
      zmsg_destroy(&msg);
      return ret;
    }
  }

  return 0;
}

const char * pk_endpoint_strerror(void)
{
  return zmq_strerror(zmq_errno());
}

int pk_endpoint_test(void)
{
  pk_endpoint_t *ept = NULL;

  /* create with invalid inputs */
  {
    ept = pk_endpoint_create("blahbloofoo", PK_ENDPOINT_PUB);
    assert(ept == NULL);

    ept = pk_endpoint_create("tcp://127.0.0.1:49010", (pk_endpoint_type)-1);
    assert(ept == NULL);
  }

  /* create server pub and connect pub */
  {
    ept = pk_endpoint_create("@tcp://127.0.0.1:49010", PK_ENDPOINT_PUB);
    assert(ept != NULL);
    pk_endpoint_type type = pk_endpoint_type_get(ept);
    assert(type == PK_ENDPOINT_PUB);
    pk_endpoint_destroy(&ept);
    assert(ept == NULL);

    ept = pk_endpoint_create(">tcp://127.0.0.1:49010", PK_ENDPOINT_PUB);
    assert(ept != NULL);
    pk_endpoint_destroy(&ept);
    assert(ept == NULL);
  }

  /* create server sub and connect sub */
  {
    ept = pk_endpoint_create("@tcp://127.0.0.1:49010", PK_ENDPOINT_SUB);
    assert(ept != NULL);
    pk_endpoint_type type = pk_endpoint_type_get(ept);
    assert(type == PK_ENDPOINT_SUB);
    pk_endpoint_destroy(&ept);
    assert(ept == NULL);

    ept = pk_endpoint_create(">tcp://127.0.0.1:49010", PK_ENDPOINT_SUB);
    assert(ept != NULL);
    int fd = pk_endpoint_poll_handle_get(ept);
    assert(fd != -1);
    pk_endpoint_destroy(&ept);
    assert(ept == NULL);
  }

  return 0;
}

