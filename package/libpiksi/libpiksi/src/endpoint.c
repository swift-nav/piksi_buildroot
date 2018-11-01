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
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/queue.h>

#include <sys/ioctl.h>
#include <linux/sockios.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/loop.h>

#include <libpiksi/endpoint.h>

// Maximum number of packets to service for one socket
#define ENDPOINT_SERVICE_MAX 32

#define IPC_PREFIX "ipc://"

// Maximum number of clients we expect to have per socket
#define MAX_CLIENTS 128
// Maximum number of clients in the listen() backlog
#define MAX_BACKLOG 128

// clang-format off
//#define DEBUG_ENDPOINT
#ifdef DEBUG_ENDPOINT
#  define ENDPOINT_DEBUG_LOG(ThePattern, ...) \
     PK_LOG_ANNO(LOG_DEBUG, ThePattern, ##__VA_ARGS__)
#  define NDEBUG_UNUSED(X)
#else
#  define ENDPOINT_DEBUG_LOG(...)
#  define NDEBUG_UNUSED(X) (void)(X)
#endif
// clang-format on

typedef struct client_node client_node_t;

typedef struct {
  pk_endpoint_t *ept;
  int fd;
  void *poll_handle;
  client_node_t *node;
} client_context_t;

struct client_node {
  client_context_t val;
  LIST_ENTRY(client_node) entries;
};

typedef LIST_HEAD(client_nodes_head, client_node) client_nodes_head_t;

struct pk_endpoint_s {
  pk_endpoint_type type;                  /**< The type socket (e.g. {pub,sub}_server, pub/sub, req/rep*/
  int sock;                               /**< The socket fd associated with this endpoint */
  int wakefd;
  bool started;
  bool nonblock;
  bool woke;
  client_nodes_head_t client_nodes_head;
  int client_count;
  pk_loop_t *loop;
  void *poll_handle;
  void *poll_handle_read;
  char path[PATH_MAX];
};

static int create_un_socket();

static int bind_un_socket(int fd, const char *path);

static int start_un_listen(const char *path, int fd);

static int connect_un_socket(int fd, const char *path);

static bool valid_socket_type_for_read(pk_endpoint_t *pk_ept);

static int recv_impl(pk_endpoint_t *ept,
                     int sock,
                     u8 *buffer,
                     size_t *length_loc,
                     bool nonblocking,
                     pk_loop_t *loop,
                     void *poll_handle,
                     client_node_t **node);

static int service_reads_base(pk_endpoint_t *ept,
                              int fd,
                              pk_loop_t *loop,
                              void *poll_handle,
                              pk_endpoint_receive_cb rx_cb,
                              void *context,
                              client_node_t **node);

static int service_reads(pk_endpoint_t *ept,
                         int fd,
                         pk_loop_t *loop,
                         void *poll_handle,
                         pk_endpoint_receive_cb rx_cb,
                         void *context);

static void send_close_socket_helper(pk_endpoint_t *ept,
                                     int sock,
                                     pk_loop_t *loop,
                                     void *poll_handle,
                                     client_node_t **node);

static int send_impl_base(pk_endpoint_t *ept,
                          int sock,
                          const u8 *data,
                          size_t length,
                          pk_loop_t *loop,
                          void *poll_handle,
                          client_node_t **node);

static int send_impl(pk_endpoint_t *ept, int sock, const u8 *data, size_t length);

static void discard_read_data(pk_loop_t *loop, client_context_t *ctx);

static void record_disconnect(pk_endpoint_t *ept, client_node_t **node);

static void handle_client_wake(pk_loop_t *loop, void *handle, int status, void *context);

static void accept_wake_handler(pk_loop_t *loop, void *handle, int status, void *context);

/**********************************************************************/
/************* pk_endpoint_create *************************************/
/**********************************************************************/

pk_endpoint_t *pk_endpoint_create(const char *endpoint, pk_endpoint_type type)
{
  ASSERT_TRACE(endpoint != NULL);

  pk_endpoint_t *pk_ept = (pk_endpoint_t *)malloc(sizeof(pk_endpoint_t));
  if (pk_ept == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate PK endpoint");
    goto failure;
  }

  *pk_ept = (pk_endpoint_t) {
    .type = type,
    .sock = -1,
    .started = false,
    .nonblock = false,
    .woke = false,
    .wakefd = -1,
    .client_count = 0,
    .loop = NULL,
    .poll_handle = NULL,
    .poll_handle_read = NULL,
  };

  strncpy(pk_ept->path, endpoint, sizeof(pk_ept->path));

  LIST_INIT(&pk_ept->client_nodes_head);

  bool do_bind = false;
  switch (pk_ept->type) {
  case PK_ENDPOINT_PUB_SERVER: do_bind = true;
  case PK_ENDPOINT_PUB: {
    pk_ept->sock = create_un_socket();
    if (pk_ept->sock < 0) {
      piksi_log(LOG_ERR, "error creating PK PUB socket: %s", pk_endpoint_strerror());
      goto failure;
    }
  } break;
  case PK_ENDPOINT_SUB_SERVER: do_bind = true;
  case PK_ENDPOINT_SUB: {
    pk_ept->sock = create_un_socket();
    if (pk_ept->sock < 0) {
      piksi_log(LOG_ERR, "error creating PK PUB/SUB socket: %s", pk_endpoint_strerror());
      goto failure;
    }
  } break;
  case PK_ENDPOINT_REP: do_bind = true;
  case PK_ENDPOINT_REQ: {
    pk_ept->sock = create_un_socket();
    if (pk_ept->sock < 0) {
      piksi_log(LOG_ERR, "error creating PK REQ/REP socket: %s", pk_endpoint_strerror());
      goto failure;
    }
  } break;
  default: {
    piksi_log(LOG_ERR, "Unsupported endpoint type");
    goto failure;
  } break;
  } // end of switch

  size_t prefix_len = strstr(endpoint, IPC_PREFIX) != NULL ? strlen(IPC_PREFIX) : 0;

  if (do_bind) {
    int rc = unlink(endpoint + prefix_len);
    if (rc != 0 && errno != ENOENT) {
      PK_LOG_ANNO(LOG_WARNING, "unlink: %s", strerror(errno));
    }
  }

  int rc = do_bind ? bind_un_socket(pk_ept->sock, endpoint + prefix_len)
                   : connect_un_socket(pk_ept->sock, endpoint + prefix_len);

  pk_ept->started = rc == 0;

  if (!pk_ept->started) {
    piksi_log(LOG_ERR,
              "Failed to %s socket: %s",
              do_bind ? "bind" : "connect",
              pk_endpoint_strerror());
    goto failure;
  }

  if (do_bind) {

    if (start_un_listen(endpoint, pk_ept->sock) < 0) goto failure;

    int rc = chmod(endpoint + prefix_len, 0777);
    if (rc != 0) {
      PK_LOG_ANNO(LOG_WARNING, "chmod: %s", strerror(errno));
    }

    pk_ept->wakefd = eventfd(0, EFD_NONBLOCK);

    if (pk_ept->wakefd < 0) {
      PK_LOG_ANNO(LOG_ERR, "eventfd: %s", strerror(errno));
      goto failure;
    }
  }

  return pk_ept;

failure:
  pk_endpoint_destroy(&pk_ept);
  return NULL;
}

/**********************************************************************/
/************* pk_endpoint_destroy ************************************/
/**********************************************************************/

void pk_endpoint_destroy(pk_endpoint_t **pk_ept_loc)
{
  if (pk_ept_loc == NULL || *pk_ept_loc == NULL) {
    return;
  }
  pk_endpoint_t *pk_ept = *pk_ept_loc;
  if (pk_ept->started) {
    while (shutdown(pk_ept->sock, SHUT_RDWR) != 0) {
      if (errno == EINTR) continue;
      piksi_log(LOG_ERR, "Failed to shutdown endpoint: %s", pk_endpoint_strerror());
      break;
    }
  }
  if (pk_ept->sock >= 0) {
    while (close(pk_ept->sock) != 0) {
      if (errno == EINTR) continue;
      piksi_log(LOG_ERR, "Failed to close socket: %s", pk_endpoint_strerror());
      break;
    }
  }
  while (!LIST_EMPTY(&pk_ept->client_nodes_head)) {
    client_node_t *node = LIST_FIRST(&pk_ept->client_nodes_head);
    LIST_REMOVE(node, entries);
    if (pk_ept->loop != NULL) {
      pk_loop_poll_remove(pk_ept->loop, node->val.poll_handle);
    }
    free(node);
  }
  if (pk_ept->wakefd >= 0) {
    while (close(pk_ept->wakefd) != 0) {
      if (errno == EINTR) continue;
      piksi_log(LOG_ERR, "Failed to close eventfd: %s", pk_endpoint_strerror());
      break;
    }
  }
  free(pk_ept);
  *pk_ept_loc = NULL;
}

/**********************************************************************/
/************* pk_endpoint_type_get ***********************************/
/**********************************************************************/

pk_endpoint_type pk_endpoint_type_get(pk_endpoint_t *pk_ept)
{
  return pk_ept->type;
}

/**********************************************************************/
/************* pk_endpoint_poll_handle_get ****************************/
/**********************************************************************/

int pk_endpoint_poll_handle_get(pk_endpoint_t *pk_ept)
{
  ASSERT_TRACE(pk_ept != NULL);

  if (pk_ept->type == PK_ENDPOINT_SUB || pk_ept->type == PK_ENDPOINT_REQ) {
    return pk_ept->sock;
  }

  if (pk_ept->type == PK_ENDPOINT_SUB_SERVER || pk_ept->type == PK_ENDPOINT_REP) {
    return pk_ept->wakefd;
  }

  /* Pub and pub server are send only, they should not need to be polled for
   * input, therefore this function should not be called on these socket
   * types.
   */
  assert(pk_ept->type == PK_ENDPOINT_PUB || pk_ept->type == PK_ENDPOINT_PUB_SERVER);

  return -1;
}

/**********************************************************************/
/************* pk_endpoint_read ***************************************/
/**********************************************************************/

ssize_t pk_endpoint_read(pk_endpoint_t *pk_ept, u8 *buffer, size_t count)
{
  ASSERT_TRACE(pk_ept != NULL);
  ASSERT_TRACE(buffer != NULL);
  ASSERT_TRACE(count > 0);

  if (!valid_socket_type_for_read(pk_ept)) {
    PK_LOG_ANNO(LOG_ERR, "invalid socket type for read");
    return -1;
  }

  size_t length = count;
  int rc = 0;

  if (pk_ept->type == PK_ENDPOINT_SUB_SERVER || pk_ept->type == PK_ENDPOINT_REP) {

    int64_t counter = 0;
    ssize_t c = read(pk_ept->wakefd, &counter, sizeof(counter));

    if (c != sizeof(counter)) {
      PK_LOG_ANNO(LOG_ERR, "invalid read size from eventfd: %zd", c);
      return -1;
    }

    ASSERT_TRACE(counter == 1);
    ASSERT_TRACE(pk_ept->woke);

    pk_ept->woke = false;

    client_node_t *node;
    LIST_FOREACH(node, &pk_ept->client_nodes_head, entries)
    {
      rc = recv_impl(pk_ept, node->val.fd, buffer, &length, pk_ept->nonblock, NULL, NULL, NULL);
      // TODO safely handle things being removed?
    }

  } else {

    rc = recv_impl(pk_ept, pk_ept->sock, buffer, &length, pk_ept->nonblock, NULL, NULL, NULL);
  }

  if (rc < 0) return rc;

  return length;
}

/**********************************************************************/
/************* pk_endpoint_receive ************************************/
/**********************************************************************/

int pk_endpoint_receive(pk_endpoint_t *pk_ept, pk_endpoint_receive_cb rx_cb, void *context)
{
  ASSERT_TRACE(pk_ept != NULL);
  ASSERT_TRACE(rx_cb != NULL);

  ASSERT_TRACE(pk_ept->nonblock);

  if (!valid_socket_type_for_read(pk_ept)) {
    PK_LOG_ANNO(LOG_ERR, "invalid socket type for read");
    return -1;
  }

  if (pk_ept->type == PK_ENDPOINT_SUB_SERVER || pk_ept->type == PK_ENDPOINT_REP) {

    int64_t counter = 0;
    ssize_t c = read(pk_ept->wakefd, &counter, sizeof(counter));

    if (c != sizeof(counter)) {
      PK_LOG_ANNO(LOG_ERR, "invalid read size from eventfd: %zd", c);
      return -1;
    }

    ASSERT_TRACE(counter == 1);
    ASSERT_TRACE(pk_ept->woke);

    pk_ept->woke = false;

    client_node_t *node;
    LIST_FOREACH(node, &pk_ept->client_nodes_head, entries)
    {
      service_reads_base(pk_ept,
                         node->val.fd,
                         pk_ept->loop,
                         node->val.poll_handle,
                         rx_cb,
                         context,
                         &node);
      // TODO safely handle things being removed?
    }

  } else {
    service_reads(pk_ept, pk_ept->sock, pk_ept->loop, pk_ept->poll_handle, rx_cb, context);
  }

  return 0;
}

/**********************************************************************/
/************* pk_endpoint_send ***************************************/
/**********************************************************************/

int pk_endpoint_send(pk_endpoint_t *pk_ept, const u8 *data, const size_t length)
{
  ASSERT_TRACE(pk_ept != NULL);
  ASSERT_TRACE(pk_ept->type != PK_ENDPOINT_SUB && pk_ept->type != PK_ENDPOINT_SUB_SERVER);

  int rc = -1;

  if (pk_ept->type == PK_ENDPOINT_PUB || pk_ept->type == PK_ENDPOINT_REQ) {
    rc = send_impl(pk_ept, pk_ept->sock, data, length);
  } else if (pk_ept->type == PK_ENDPOINT_PUB_SERVER || pk_ept->type == PK_ENDPOINT_REP) {
    client_node_t *node;
    LIST_FOREACH(node, &pk_ept->client_nodes_head, entries)
    {
      rc = send_impl_base(pk_ept,
                          node->val.fd,
                          data,
                          length,
                          pk_ept->loop,
                          node->val.poll_handle,
                          &node);
      // TODO safely handle things being removed?
    }
  }

  return rc;
}

/**************************************************************************/
/************* pk_endpoint_strerror ***************************************/
/**************************************************************************/

const char *pk_endpoint_strerror(void)
{
  return strerror(errno);
}

/**************************************************************************/
/************* pk_endpoint_set_non_blocking *******************************/
/**************************************************************************/

int pk_endpoint_set_non_blocking(pk_endpoint_t *pk_ept)
{
  int status = -1;

  if ((status = fcntl(pk_ept->sock, F_SETFL, fcntl(pk_ept->sock, F_GETFL, 0) | O_NONBLOCK))) {
    PK_LOG_ANNO(LOG_ERR, "fcntl error: %s", strerror(errno));
    return -1;
  }

  pk_ept->nonblock = true;

  return 0;
}

/**************************************************************************/
/************* pk_endpoint_accept *****************************************/
/**************************************************************************/

int pk_endpoint_accept(pk_endpoint_t *pk_ept)
{
  int cl;

  if ((cl = accept(pk_ept->sock, NULL, NULL)) == -1) {
    PK_LOG_ANNO(LOG_ERR, "accept error: %s", strerror(errno));
    return -1;
  }

  return cl;
}

/**************************************************************************/
/************* pk_endpoint_loop_add ***************************************/
/**************************************************************************/

int pk_endpoint_loop_add(pk_endpoint_t *pk_ept, pk_loop_t *loop, void *poll_handle_read)
{
  if (pk_endpoint_set_non_blocking(pk_ept) < 0) return -1;

  if (pk_ept->type == PK_ENDPOINT_SUB || pk_ept->type == PK_ENDPOINT_PUB
      || pk_ept->type == PK_ENDPOINT_REQ) {

    ASSERT_TRACE(poll_handle_read != NULL);
    pk_ept->poll_handle = poll_handle_read;

    return 0;
  }

  ASSERT_TRACE(loop != NULL);

  if (pk_ept->loop == loop) {

    /* If we're a server style SUB/REP socket, pk_loop_endpoint_reader_add will
     * pass a poll_handle of NULL because someone else should be handling the
     * read events for the socket.
     */

    ASSERT_TRACE(poll_handle_read != NULL);
    ASSERT_TRACE(pk_ept->poll_handle_read == NULL);

    ASSERT_TRACE(pk_ept->type == PK_ENDPOINT_SUB_SERVER || pk_ept->type == PK_ENDPOINT_REP);

    pk_ept->poll_handle_read = poll_handle_read;

    return 0;
  }

  ASSERT_TRACE(pk_ept->loop == NULL);
  ASSERT_TRACE(pk_ept->poll_handle == NULL);

  if (pk_endpoint_set_non_blocking(pk_ept) < 0) return -1;

  pk_ept->loop = loop;

  ASSERT_TRACE(poll_handle_read == NULL);
  ASSERT_TRACE(pk_ept->poll_handle == NULL);

  pk_ept->poll_handle = pk_loop_poll_add(loop, pk_ept->sock, accept_wake_handler, pk_ept);

  return pk_ept->poll_handle != NULL ? 0 : -1;
}

/**************************************************************************/
/************* Helpers ****************************************************/
/**************************************************************************/

static int create_un_socket()
{
  int fd = -1;

  if ((fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
    PK_LOG_ANNO(LOG_ERR, "socket error: %s", strerror(errno));
    return -1;
  }

  return fd;
}

static int bind_un_socket(int fd, const char *path)
{
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind error");
    return -1;
  }

  return 0;
}

static int start_un_listen(const char *path, int fd)
{
  if (listen(fd, MAX_BACKLOG) == -1) {

    PK_LOG_ANNO(LOG_ERR, "listen() error for socket: %s (path: %s)", strerror(errno), path);
    return -1;
  }

  return 0;
}

static int connect_un_socket(int fd, const char *path)
{
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    PK_LOG_ANNO(LOG_ERR, "connect error: %s (path: %s)", strerror(errno), path);
    return -1;
  }

  return 0;
}

static bool valid_socket_type_for_read(pk_endpoint_t *pk_ept)
{
  if (pk_ept->type == PK_ENDPOINT_SUB) return true;

  if (pk_ept->type == PK_ENDPOINT_SUB_SERVER) return true;

  if (pk_ept->type == PK_ENDPOINT_REP) return true;

  if (pk_ept->type == PK_ENDPOINT_REQ) return true;

  return false;
}

static int recv_impl(pk_endpoint_t *ept,
                     int sock,
                     u8 *buffer,
                     size_t *length_loc,
                     bool nonblocking,
                     pk_loop_t *loop,
                     void *poll_handle,
                     client_node_t **node)
{
  int err = 0;
  int length = 0;

  struct iovec iov[1] = {0};
  struct msghdr msg = {0};

  iov[0].iov_base = buffer;
  iov[0].iov_len = *length_loc;

  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  while (1) {

    length = recvmsg(sock, &msg, 0);

    if (length >= 0) {
      if (length == 0) {
        ENDPOINT_DEBUG_LOG("socket closed");
        if (loop != NULL) {
          ASSERT_TRACE(poll_handle != NULL);
          pk_loop_poll_remove(loop, poll_handle);
        }
        if (node != NULL) record_disconnect(ept, node);
        shutdown(sock, SHUT_RDWR);
        close(sock);
      }
      // TODO: we should probably auto reconnect here if we're a PUB/SUB/REQ
      //   (non-server socket).
      break;
    }

    if (errno == EINTR) {
      /* Retry if interrupted */
      ENDPOINT_DEBUG_LOG("got EINTR from recvmsg: %s", strerror(errno));
      continue;
    }

    if (nonblocking && errno == EAGAIN) {
      // An "expected" error, don't need to report an error
      return PKE_EAGAIN;
    }

    if ((err = errno) != ENOTCONN) {
      PK_LOG_ANNO(LOG_ERR, "recvmsg error: %d (%s)", err, strerror(err));
    }

    return err == ENOTCONN ? PKE_NOT_CONN : PKE_ERROR;
  }

  *length_loc = (size_t)length;

  return PKE_SUCCESS;
}

static int service_reads_base(pk_endpoint_t *ept,
                              int fd,
                              pk_loop_t *loop,
                              void *poll_handle,
                              pk_endpoint_receive_cb rx_cb,
                              void *context,
                              client_node_t **node)
{
  for (size_t i = 0; i < ENDPOINT_SERVICE_MAX; i++) {
    u8 buffer[4096] = {0};
    size_t length = sizeof(buffer);
    int rc = recv_impl(ept, fd, buffer, &length, true, loop, poll_handle, node);
    if (rc < 0) {
      if (rc == PKE_EAGAIN || rc == PKE_NOT_CONN) break;
      piksi_log(LOG_ERR, "failed to receive message");
      return -1;
    }
    if (length == 0) break;
    bool stop = rx_cb(buffer, length, context) != 0;
    if (stop) break;
  }
  return 0;
}

static int service_reads(pk_endpoint_t *ept,
                         int fd,
                         pk_loop_t *loop,
                         void *poll_handle,
                         pk_endpoint_receive_cb rx_cb,
                         void *context)
{
  return service_reads_base(ept, fd, loop, poll_handle, rx_cb, context, NULL);
}

static void send_close_socket_helper(pk_endpoint_t *ept,
                                     int sock,
                                     pk_loop_t *loop,
                                     void *poll_handle,
                                     client_node_t **node)
{
  if (loop != NULL) {

    ASSERT_TRACE(poll_handle != NULL);
    ASSERT_TRACE(node != NULL);

    pk_loop_poll_remove(loop, poll_handle);
    if (node != NULL) record_disconnect(ept, node);
  }

  shutdown(sock, SHUT_RDWR);
  close(sock);
}

static int send_impl_base(pk_endpoint_t *ept,
                          int sock,
                          const u8 *data,
                          const size_t length,
                          pk_loop_t *loop,
                          void *poll_handle,
                          client_node_t **node)
{
  struct iovec iov[1] = {0};
  struct msghdr msg = {0};

  iov[0].iov_base = (u8 *)data;
  iov[0].iov_len = length;

  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  while (1) {

    int written = sendmsg(sock, &msg, 0);
    int error = errno;

    if (written != -1) {
      /* Break on success */
      ASSERT_TRACE(written == length);
      return 0;
    }

    if (error == EAGAIN || error == EWOULDBLOCK) {

      int queued_input = -1;
      int error = ioctl(sock, SIOCINQ, &queued_input);

      if (error < 0) PK_LOG_ANNO(LOG_WARNING, "unable to read SIOCINQ: %s", strerror(errno));

      int queued_output = -1;
      error = ioctl(sock, SIOCOUTQ, &queued_output);

      if (error < 0) PK_LOG_ANNO(LOG_WARNING, "unable to read SIOCOUTQ: %s", strerror(errno));

      PK_LOG_ANNO(LOG_WARNING,
                  "sendmsg returned EAGAIN, dropping %d bytes "
                  "(path: %s, node: %p, queued input: %d, queued output: %d)",
                  length,
                  ept->path,
                  node,
                  queued_input,
                  queued_output);

      send_close_socket_helper(ept, sock, loop, poll_handle, node);

      return 0;
    }

    if (error == EINTR) {
      /* Retry if interrupted */
      ENDPOINT_DEBUG_LOG("sendmsg returned with EINTR");
      continue;
    }

    send_close_socket_helper(ept, sock, loop, poll_handle, node);

    if (error != EPIPE && error != ECONNRESET) {
      PK_LOG_ANNO(LOG_ERR, "error in sendmsg: %s", strerror(error));
    }

    /* Return error */
    return -1;
  }
}

static int send_impl(pk_endpoint_t *ept, int sock, const u8 *data, const size_t length)
{
  return send_impl_base(ept, sock, data, length, NULL, NULL, NULL);
}

static void discard_read_data(pk_loop_t *loop, client_context_t *ctx)
{
  u8 read_buf[4096];
  size_t length = sizeof(read_buf);

  for (int count = 0; count < ENDPOINT_SERVICE_MAX; count++) {

    if (ctx == NULL || ctx->node == NULL) break;

    pk_endpoint_t *ept = ctx->ept;
    int fd = ctx->fd;
    void *poll_handle = ctx->poll_handle;
    client_node_t *node = ctx->node;

    if (recv_impl(ept, fd, read_buf, &length, true, loop, poll_handle, &node) != 0) {
      break;
    }
  }
}

static void record_disconnect(pk_endpoint_t *ept, client_node_t **node)
{
  ASSERT_TRACE(ept != NULL);
  ASSERT_TRACE(node != NULL && *node != NULL);

  --ept->client_count;
  LIST_REMOVE(*node, entries);

  free(*node);
  *node = NULL;

  if (ept->client_count < 0) {
    PK_LOG_ANNO(LOG_ERR | LOG_SBP, "client count is negative (count: %d)", ept->client_count);
  }
}

static void handle_client_wake(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)handle;

  client_context_t *client_context = (client_context_t *)context;

  if ((status & LOOP_ERROR) || (status & LOOP_DISCONNECTED)) {

    ENDPOINT_DEBUG_LOG("client disconnected: %s (%08x)", pk_loop_describe_status(status), status);

    close(client_context->fd);
    record_disconnect(client_context->ept, &client_context->node);

    return;
  }

  if ((status & LOOP_READ) && client_context->ept->type == PK_ENDPOINT_PUB_SERVER) {

    piksi_log(LOG_WARNING, "discarding read data from pub server");
    discard_read_data(loop, client_context);

    return;
  }

  // Don't wake-up loop again if one is already pending
  if (client_context->ept->woke) return;

  client_context->ept->woke = true;

  int64_t incr_value = 1;
  write(client_context->ept->wakefd, &incr_value, sizeof(incr_value));
}

static void accept_wake_handler(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)handle;

  ASSERT_TRACE(loop != NULL);

  if (!(status & LOOP_READ) && status != LOOP_SUCCESS) {

    if (status & LOOP_ERROR) {
      PK_LOG_ANNO(LOG_ERR,
                  "status: %s; error: %s",
                  pk_loop_describe_status(status),
                  pk_loop_last_error(loop));
    } else {
      PK_LOG_ANNO(LOG_ERR, "status: %s", pk_loop_describe_status(status));
    }

    return;
  }

  pk_endpoint_t *ept = (pk_endpoint_t *)context;
  ASSERT_TRACE(ept != NULL);

  client_node_t *client_node = (client_node_t *)malloc(sizeof(client_node_t));

  if (client_node == NULL) {

    piksi_log(LOG_WARNING, "unable to add new client, closing connection");
    int clientfd = pk_endpoint_accept(ept);

    shutdown(clientfd, SHUT_RDWR);
    close(clientfd);

    return;
  }

  LIST_INSERT_HEAD(&ept->client_nodes_head, client_node, entries);

  client_context_t *client_context = &client_node->val;

  ENDPOINT_DEBUG_LOG("new client_count: %d; path: %s", ept->client_count + 1, ept->path);

  int clientfd = pk_endpoint_accept(ept);

  ENDPOINT_DEBUG_LOG("new client_fd: %d", clientfd);

  client_context->fd = clientfd;
  client_context->ept = ept;
  client_context->node = client_node;

  if (fcntl(clientfd, F_SETFL, fcntl(clientfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
    PK_LOG_ANNO(LOG_WARNING, "fcntl error: %s", strerror(errno));
  }

  if (++ept->client_count > MAX_CLIENTS) {
    piksi_log(LOG_WARNING | LOG_SBP,
              "client count exceeding expected maximum: %zu",
              ept->client_count);
  }

  client_context->poll_handle =
    pk_loop_poll_add(loop, clientfd, handle_client_wake, client_context);

  ASSERT_TRACE(client_context->poll_handle != NULL);
}


