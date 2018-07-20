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
#include <fcntl.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#if 0
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/reqrep.h>
#endif

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/loop.h>

#include <libpiksi/endpoint.h>

// Maximum number of packets to service for one socket
#define EPT_SVC_MAX 32

#define IPC_PREFIX "ipc://"

#define MAX_CLIENTS 128

typedef struct {
	pk_endpoint_t *ept;
	int fd;
} client_context_t;

struct pk_endpoint_s {
  pk_endpoint_type type;
  int sock;
  int wakefd;
  int eid;
	bool nonblock;
	client_context_t clients[MAX_CLIENTS];
	size_t client_count;
};

static void accept_wake_handler(pk_loop_t *loop, void *handle, void *context);

static int create_un_socket() {

	int fd = -1;

  if ( (fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
    perror("socket error");
		return -1;
  }

	return fd;
}

static int bind_un_socket(int fd, const char* path) {

  struct sockaddr_un addr = { .sun_family = AF_UNIX };
  strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

  if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("bind error");
		return -1;
  }

	return 0;
}

static int start_un_listen(int fd) {

  if (listen(fd, 5) == -1) {
    perror("listen error");
		return -1;
  }

	return 0;
}

static int connect_un_socket(int fd, const char* path) {

  struct sockaddr_un addr = { .sun_family = AF_UNIX };
  strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("connect error");
		return -1;
  }

	return 0;
}

pk_endpoint_t * pk_endpoint_create(const char *endpoint, pk_endpoint_type type)
{
  assert(endpoint != NULL);

  pk_endpoint_t *pk_ept = (pk_endpoint_t *)malloc(sizeof(pk_endpoint_t));
  if (pk_ept == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate PK endpoint");
    goto failure;
  }

  pk_ept->type = type;
  pk_ept->sock = -1;
  pk_ept->eid = -1;
  pk_ept->nonblock = false;
	pk_ept->client_count = 0;

  bool do_bind = false;
  switch (pk_ept->type)
  {
  case PK_ENDPOINT_PUB_SERVER:
    do_bind = true;
  case PK_ENDPOINT_PUB:
  {
    pk_ept->sock = create_un_socket();
    if (pk_ept->sock < 0) {
      piksi_log(LOG_ERR, "error creating PK PUB socket: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
  } break;
  case PK_ENDPOINT_SUB_SERVER:
    do_bind = true;
  case PK_ENDPOINT_SUB:
  {
    pk_ept->sock = create_un_socket();
    if (pk_ept->sock < 0) {
      piksi_log(LOG_ERR, "error creating PK SUB socket: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
  } break;
  case PK_ENDPOINT_REQ:
	#if 0
  {
    pk_ept->sock = nn_socket(AF_SP, NN_REQ);
    if (pk_ept->sock < 0) {
      piksi_log(LOG_ERR, "error creating PK REQ socket: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
  } break;
	#endif
		// TODO: implement
    goto failure;
  case PK_ENDPOINT_REP:
	#if 0
  {
    do_bind = true;
    pk_ept->sock = nn_socket(AF_SP, NN_REP);
    if (pk_ept->sock < 0) {
      piksi_log(LOG_ERR, "error creating PK REP socket: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
  } break;
	#endif
		// TODO: implement
    goto failure;
  default:
  {
    piksi_log(LOG_ERR, "Unsupported endpoint type");
    goto failure;
  } break;
  } // end of switch

  size_t prefix_len = strlen(IPC_PREFIX);
  pk_ept->eid = do_bind ? bind_un_socket(pk_ept->sock, endpoint+prefix_len)
                        : connect_un_socket(pk_ept->sock, endpoint+prefix_len);

  if (pk_ept->eid < 0) {
    piksi_log(LOG_ERR, "Failed to %s socket: %s",
                       do_bind ? "bind" : "connect",
                       pk_endpoint_strerror());
    goto failure;
  }

  if (do_bind) {

    if (strlen(endpoint) > prefix_len) {
      int rc = chmod(endpoint + prefix_len, 0777);
      if (rc != 0) {
        piksi_log(LOG_WARNING, "%s: chmod: %s", __FUNCTION__, strerror(errno));
      }
    }

		start_un_listen(pk_ept->sock);
		pk_ept->wakefd = eventfd(0, EFD_NONBLOCK);
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
    while (shutdown(pk_ept->sock, SHUT_RDWR) != 0) {
      piksi_log(LOG_ERR, "Failed to shutdown endpoint: %s", pk_endpoint_strerror());
      // retry if EINTR, others likely mean we should exit
      if (errno == EINTR) {
        continue;
      }
      break;
    }
  }
  if (pk_ept->sock >= 0) {
    while (close(pk_ept->sock) != 0) {
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

  assert(pk_ept->type != PK_ENDPOINT_PUB);
	assert(pk_ept->type != PK_ENDPOINT_PUB_SERVER);

	// TODO: handle req/rep sockets
	return pk_ept->type == PK_ENDPOINT_SUB ? pk_ept->sock : pk_ept->wakefd;
}

static int recv_msg_impl(int sock, u8 *buffer, size_t *length_loc, bool nonblocking)
{
  int length = 0;

	struct iovec iov[1] = {0};
	struct msghdr msg = {0};

	iov[0].iov_base = buffer;
	iov[0].iov_len  = *length_loc;

	msg.msg_iov     = iov;
	msg.msg_iovlen  = 1;

  while (1) {
    length = recvmsg(sock, &msg, 0);
//		piksi_log(LOG_DEBUG, "%s: recvmsg = %d (%s:%d)", __FUNCTION__, length, __FILE__, __LINE__);
    if (length >= 0) {
      if (length == 0) piksi_log(LOG_WARNING, "Empty message received");
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      piksi_log(LOG_DEBUG, "Retry recv on EINTR");
      continue;
    } else if (nonblocking && errno == EAGAIN) {
//			piksi_log(LOG_DEBUG, "%s: recv_msg_impl: EAGAIN (%s:%d)", __FUNCTION__, __FILE__, __LINE__);
      // An "expected" error, don't need to report an error
      return -1;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in recv(): %s", pk_endpoint_strerror());
      return -1;
    }
  }

  *length_loc = (size_t) length;

  return 0;
}

#if 0
/**
 * @brief pk_endpoint_receive_msg - helper to retrieve a single message
 *
 * @param pk_ept: pointer to the endpoint context
 * @param buffer: pointer to existing buffer - OR - double pointer to receive allocated msg
 * @param length: size of existing buffer - OR - pointer to receive msg length
 * @param nonblocking: if the call should be nonblocking
 *
 * @return                  The operation result.
 * @retval 0                Receive operation was successful.
 * @retval -1               An error occurred.
 */
static int pk_endpoint_recv_msg(pk_endpoint_t *pk_ept, u8 *buffer, size_t *length_loc, bool nonblocking)
{
  assert(pk_ept != NULL);
	return recv_msg_impl(pk_ept->sock, buffer, length_loc, nonblocking);
}
#endif

ssize_t pk_endpoint_read(pk_endpoint_t *pk_ept, u8 *buffer, size_t count)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_PUB || pk_ept->type != PK_ENDPOINT_PUB_SERVER);

  assert(buffer != NULL);
  assert(count > 0);

  size_t length = count;
  if (recv_msg_impl(pk_ept->sock, buffer, &length, pk_ept->nonblock) != 0) {
    piksi_log(LOG_ERR, "failed to receive message");
    return -1;
  }

  return length;
}

static int service_reads(int fd, pk_endpoint_receive_cb rx_cb, void *context)
{
	for (size_t i = 0; i < EPT_SVC_MAX; i++) {
    u8 buffer[4096];
    size_t length = sizeof(buffer);
    if (recv_msg_impl(fd, buffer, &length, true) != 0) {
      if (errno == EWOULDBLOCK) break;
      piksi_log(LOG_ERR, "failed to receive message");
      return -1;
    }
    bool stop = rx_cb(buffer, length, context) != 0;
    if (stop) break;
  }
	return 0;
}

int pk_endpoint_receive(pk_endpoint_t *pk_ept, pk_endpoint_receive_cb rx_cb, void *context)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_PUB || pk_ept->type != PK_ENDPOINT_PUB_SERVER);
	assert(pk_ept->nonblock);
  assert(rx_cb != NULL);

	if (pk_ept->type == PK_ENDPOINT_SUB_SERVER) {
//		piksi_log(LOG_DEBUG, "%s: reading from clients (%s:%d)", __FUNCTION__, __FILE__, __LINE__);
		int64_t counter = 0;
		read(pk_ept->wakefd, &counter, sizeof(counter));
		for (size_t i = 0; i < pk_ept->client_count; i++) {
			if (service_reads(pk_ept->clients[i].fd, rx_cb, context) != 0)
				break;
		}
	} else {
		service_reads(pk_ept->sock, rx_cb, context);
	}

  return 0;
}

static int send_impl(int sock, const u8 *data, const size_t length)
{
	struct iovec iov[1] = {0};
	struct msghdr msg = {0};

	iov[0].iov_base = (u8*) data;
	iov[0].iov_len  = length;

	msg.msg_iov     = iov;
	msg.msg_iovlen  = 1;

  while (1) {
    int written = sendmsg(sock, &msg, 0);
    if (written != -1) {
      /* Break on success */
      assert(written == length);
      return 0;
    } else if (errno == EAGAIN) {
      /* Retry... */
      piksi_log(LOG_DEBUG, "%s: send returned with EAGAIN (%s:%d)", __FUNCTION__, __FILE__, __LINE__);
      continue;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in send(): %s", pk_endpoint_strerror());
      return -1;
    }
  }
}

int pk_endpoint_send(pk_endpoint_t *pk_ept, const u8 *data, const size_t length)
{
  assert(pk_ept != NULL);
  assert(pk_ept->type != PK_ENDPOINT_SUB || pk_ept->type != PK_ENDPOINT_SUB_SERVER);

	if (pk_ept->type == PK_ENDPOINT_PUB) {
		send_impl(pk_ept->sock, data, length);
	} else if (pk_ept->type == PK_ENDPOINT_PUB_SERVER) {
//    piksi_log(LOG_DEBUG, "%s: sending to clients", __FUNCTION__, pk_endpoint_strerror());
		for (int idx = 0; idx < pk_ept->client_count; idx++) {
			send_impl(pk_ept->clients[idx].fd, data, length);
		}
	}
}

const char * pk_endpoint_strerror(void)
{
  return strerror(errno);
}

int pk_endpoint_set_non_blocking(pk_endpoint_t *pk_ept)
{
	int status = -1;

	if ( (status = fcntl(pk_ept->sock, F_SETFL, fcntl(pk_ept->sock, F_GETFL, 0) | O_NONBLOCK)) ) {
    perror("fcntl error");
		return -1;
	}

	pk_ept->nonblock = true;

	return 0;
}

int pk_endpoint_accept(pk_endpoint_t *pk_ept)
{	
	int cl;

	if ( (cl = accept(pk_ept->sock, NULL, NULL)) == -1) {
		piksi_log(LOG_ERR, "accept error: %s", strerror(errno));
		return -1;
  }

	return cl;
}

static void client_read(pk_loop_t *loop, void *handle, void *context)
{
	u8 read_buf[4096];
	size_t length = sizeof(read_buf);

	client_context_t *client_context = (client_context_t *)context;

//	piksi_log(LOG_DEBUG, "%s: waking up for client read: fd = %d (%s:%d)",
//					  __FUNCTION__, client_context->fd, __FILE__, __LINE__);

	if (client_context->ept->type == PK_ENDPOINT_PUB_SERVER) {
		piksi_log(LOG_WARNING, "discarding read data from pub server");
		for (int count = 0; count < EPT_SVC_MAX; count++) {
			if (recv_msg_impl(client_context->fd, read_buf, &length, true) != 0) {
				if (errno == EWOULDBLOCK) break;
				break;
			}
		}
		return;
	}

	int64_t incr_value = 1;
	write(client_context->ept->wakefd, &incr_value, sizeof(incr_value));
}

static void accept_wake_handler(pk_loop_t *loop, void *handle, void *context)
{
	pk_endpoint_t *ept = (pk_endpoint_t *)context;
	client_context_t *client_context = &ept->clients[ept->client_count++];

	piksi_log(LOG_DEBUG, "%s: got new client; client_count: %d; client_context: %p (%s:%d)",
					  __FUNCTION__, ept->client_count, client_context, __FILE__, __LINE__);

	int clientfd = pk_endpoint_accept(ept);

	piksi_log(LOG_DEBUG, "%s: client_fd: %d (%s:%d)",
					  __FUNCTION__, clientfd, __FILE__, __LINE__);

	client_context->fd = clientfd;
	client_context->ept = ept;

	int status = -1;
	if ( (status = fcntl(clientfd, F_SETFL, fcntl(clientfd, F_GETFL, 0) | O_NONBLOCK)) ) {
    perror("fcntl error");
	}

	assert( loop != NULL );

	pk_loop_poll_add(loop, clientfd, client_read, client_context);
}

pk_loop_cb pk_endpoint_start_server(pk_endpoint_t *pk_ept, pk_loop_t *loop)
{
	pk_endpoint_set_non_blocking(pk_ept);

	assert( pk_ept->type == PK_ENDPOINT_PUB_SERVER || pk_ept->type == PK_ENDPOINT_SUB_SERVER );
	assert( loop != NULL );

	pk_loop_poll_add(loop, pk_ept->sock, accept_wake_handler, pk_ept);
}
