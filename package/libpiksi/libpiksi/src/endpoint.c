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

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/reqrep.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>

#include <libpiksi/endpoint.h>

struct pk_endpoint_s {
  pk_endpoint_type type;
  int nn_sock;
  int eid;
};

// Maximum number of packets to service for one socket
#define EPT_SVC_MAX 128

#define IPC_PREFIX "ipc://"

pk_endpoint_t * pk_endpoint_create(const char *endpoint, pk_endpoint_type type)
{
  assert(endpoint != NULL);

  pk_endpoint_t *pk_ept = (pk_endpoint_t *)malloc(sizeof(pk_endpoint_t));
  if (pk_ept == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate PK endpoint");
    goto failure;
  }

  pk_ept->type = type;
  pk_ept->nn_sock = -1;
  pk_ept->eid = -1;
  bool do_bind = false;
  switch (pk_ept->type)
  {
  case PK_ENDPOINT_PUB_SERVER:
    do_bind = true;
  case PK_ENDPOINT_PUB:
  {
    pk_ept->nn_sock = nn_socket(AF_SP, NN_PUB);
    if (pk_ept->nn_sock < 0) {
      piksi_log(LOG_ERR, "error creating PK PUB socket: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
  } break;
  case PK_ENDPOINT_SUB_SERVER:
    do_bind = true;
  case PK_ENDPOINT_SUB:
  {
    pk_ept->nn_sock = nn_socket(AF_SP, NN_SUB);
    if (pk_ept->nn_sock < 0) {
      piksi_log(LOG_ERR, "error creating PK SUB socket: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
    if (nn_setsockopt(pk_ept->nn_sock, NN_SUB, NN_SUB_SUBSCRIBE, "", 0) != 0) {
      piksi_log(LOG_ERR, "error assigning subscribe setting: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
  } break;
  case PK_ENDPOINT_REQ:
  {
    pk_ept->nn_sock = nn_socket(AF_SP, NN_REQ);
    if (pk_ept->nn_sock < 0) {
      piksi_log(LOG_ERR, "error creating PK REQ socket: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
  } break;
  case PK_ENDPOINT_REP:
  {
    do_bind = true;
    pk_ept->nn_sock = nn_socket(AF_SP, NN_REP);
    if (pk_ept->nn_sock < 0) {
      piksi_log(LOG_ERR, "error creating PK REP socket: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
  } break;
  default:
  {
    piksi_log(LOG_ERR, "Unsupported endpoint type");
    goto failure;
  } break;
  } // end of switch

  pk_ept->eid = do_bind ? nn_bind(pk_ept->nn_sock, endpoint)
                        : nn_connect(pk_ept->nn_sock, endpoint);
  if (pk_ept->eid < 0) {
    piksi_log(LOG_ERR, "Failed to %s socket: %s",
                       do_bind ? "bind" : "connect",
                       pk_endpoint_strerror());
    goto failure;
  }

  if (do_bind) {
    size_t prefix_len = strlen(IPC_PREFIX);
    if (strlen(endpoint) > prefix_len) {
      int rc = chmod(endpoint + prefix_len, 0777);
      if (rc != 0) {
        piksi_log(LOG_WARNING, "%s: chmod: %s", __FUNCTION__, strerror(errno));
      }
    }
  }

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
  if (pk_ept->eid >= 0) {
    while (nn_shutdown(pk_ept->nn_sock, pk_ept->eid) != 0) {
      piksi_log(LOG_ERR, "Failed to shutdown endpoint: %s", pk_endpoint_strerror());
      // retry if EINTR, others likely mean we should exit
      if (errno == EINTR) {
        continue;
      }
      break;
    }
  }
  if (pk_ept->nn_sock >= 0) {
    while (nn_close(pk_ept->nn_sock) != 0) {
      piksi_log(LOG_ERR, "Failed to close socket: %s", pk_endpoint_strerror());
      // retry if EINTR, EBADF means it wasn't valid in the first place
      if (errno == EINTR) {
        continue;
      }
      break;
    }
  }
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
  assert(pk_ept->type != PK_ENDPOINT_PUB || pk_ept->type != PK_ENDPOINT_PUB_SERVER);

  int recv_sock = -1;
  size_t recv_sock_size = sizeof(int);
  if (nn_getsockopt(pk_ept->nn_sock, NN_SOL_SOCKET, NN_RCVFD, &recv_sock, &recv_sock_size) != 0) {
    piksi_log(LOG_ERR, "Failed to get poll fd: %s", pk_endpoint_strerror());
    return -1;
  }
  return recv_sock;
}

/**
 * @brief pk_endpoint_receive_nn_msg - helper to retrieve a single message
 *
 * @param pk_ept: pointer to the endpoint context
 * @param buffer: pointer to existing buffer - OR - double pointer to receive allocated msg
 * @param length: size of existing buffer - OR - pointer to receive msg length
 * @param nonblocking: if the call should be nonblocking
 *
 * @note    The buffer and length options mirror the nn_recv options
 *
 * @return                  The operation result.
 * @retval 0                Receive operation was successful.
 * @retval -1               An error occurred.
 */
static int pk_endpoint_receive_nn_msg(pk_endpoint_t *pk_ept, void *buffer_loc, size_t *length_loc, bool nonblocking)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_PUB || pk_ept->type != PK_ENDPOINT_PUB_SERVER);

  int length = 0;

  while (1) {
    length = nn_recv(pk_ept->nn_sock, buffer_loc, *length_loc, nonblocking ? NN_DONTWAIT : 0);
    if (length >= 0) {
      if (length == 0) piksi_log(LOG_WARNING, "Empty message received");
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      piksi_log(LOG_DEBUG, "Retry recv on EINTR");
      continue;
    } else if (nonblocking && errno == EAGAIN) {
      // An "expected" error, don't need to report an error
      return -1;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in nn_recv(): %s", pk_endpoint_strerror());
      return -1;
    }
  }

  *length_loc = (size_t) length;
  return 0;
}

ssize_t pk_endpoint_read(pk_endpoint_t *pk_ept, u8 *buffer, size_t count)
{
  assert(pk_ept != NULL);
  assert(buffer != NULL);

  size_t length = count;
  if (pk_endpoint_receive_nn_msg(pk_ept, buffer, &length, false) != 0) {
    piksi_log(LOG_ERR, "failed to receive nn_msg");
    return -1;
  }

  return length;
}

int pk_endpoint_receive(pk_endpoint_t *pk_ept, pk_endpoint_receive_cb rx_cb, void *context)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_PUB || pk_ept->type != PK_ENDPOINT_PUB_SERVER);
  assert(rx_cb != NULL);

  for (size_t i = 0; i < EPT_SVC_MAX; i++) {

    u8 *buffer = NULL;
    size_t length = NN_MSG;

    if (pk_endpoint_receive_nn_msg(pk_ept, (void *)&buffer, &length, true) != 0) {
      if (errno == EAGAIN) break;
      piksi_log(LOG_ERR, "failed to receive nn_msg");
      return -1;
    }

    rx_cb(buffer, length, context);
    nn_freemsg(buffer);
  }

  return 0;
}

int pk_endpoint_send(pk_endpoint_t *pk_ept, const u8 *data, const size_t length)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_SUB || pk_ept->type != PK_ENDPOINT_SUB_SERVER);

  while (1) {
    int written = nn_send(pk_ept->nn_sock, data, length, 0);
    if (written != -1) {
      /* Break on success */
      assert(written == length);
      return 0;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in nn_send(): %s", pk_endpoint_strerror());
      return -1;
    }
  }
}

const char * pk_endpoint_strerror(void)
{
  return nn_strerror(errno);
}

