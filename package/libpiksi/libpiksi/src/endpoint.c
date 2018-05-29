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
    // error condition!
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

ssize_t pk_endpoint_read(pk_endpoint_t *pk_ept, u8 *buffer, size_t count)
{
  assert(pk_ept != NULL);
  assert(buffer != NULL);

  zmsg_t * msg = pk_endpoint_receive_zmsg(pk_ept);
  if (msg == NULL) {
    return -1;
  }

  size_t buffer_index = 0;
  zframe_t *frame = zmsg_first(msg);
  while (frame != NULL) {
    const void *data = zframe_data(frame);
    size_t size = zframe_size(frame);

    size_t copy_length = buffer_index + size <= count ?
        size : count - buffer_index;

    if (copy_length > 0) {
      memcpy(&((uint8_t *)buffer)[buffer_index], data, copy_length);
      buffer_index += copy_length;
    }

    frame = zmsg_next(msg);
  }

  zmsg_destroy(&msg);
  assert(msg == NULL);

  return buffer_index;
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

    zframe_t *frame;
    for (frame = zmsg_first(msg); frame != NULL; frame = zmsg_next(msg)) {
      rx_cb(zframe_data(frame), zframe_size(frame), context);
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
