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
#include <time.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/queue.h>

#include <linux/sockios.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/loop.h>

#include <libpiksi/endpoint.h>

/* Maximum number of reads to service for one socket */
#define ENDPOINT_SERVICE_MAX (32u)

/* 300 * 100ms = 30s of retries */
#define CONNECT_RETRIES_MAX (300u)
#define CONNECT_RETRY_SLEEP_MS (100u)

#define MS_TO_NS(MS) ((MS)*1e6)

#define READ_BUF_SIZE 4096

#define IPC_PREFIX "ipc://"

/* Maximum number of clients we expect to have per socket */
#define MAX_CLIENTS 128
/* Maximum number of clients in the listen() backlog */
#define MAX_BACKLOG 128

/* clang-format off */
/* #define DEBUG_ENDPOINT */
#ifdef DEBUG_ENDPOINT
#  define ENDPOINT_DEBUG_LOG(ThePattern, ...) \
     PK_LOG_ANNO(LOG_DEBUG, ThePattern, ##__VA_ARGS__)
#  define NDEBUG_UNUSED(X)
#else
#  define ENDPOINT_DEBUG_LOG(...)
#  define NDEBUG_UNUSED(X) (void)(X)
#endif
/* clang-format on */

typedef struct client_node client_node_t;
typedef struct removed_node removed_node_t;

typedef struct {
  pk_endpoint_t *ept;
  int handle;
  void *poll_handle;
  client_node_t *node;
} client_context_t;

struct client_node {
  client_context_t val;
  LIST_ENTRY(client_node) entries;
};

struct removed_node {
  client_node_t *client_node;
  LIST_ENTRY(removed_node) entries;
};

typedef LIST_HEAD(client_nodes_head, client_node) client_nodes_head_t;
typedef LIST_HEAD(removed_nodes_head, removed_node) removed_nodes_head_t;

struct pk_endpoint_s {
  pk_endpoint_type type; /**< The type socket (e.g. {pub,sub}_server, pub/sub, req/rep*/
  int sock;              /**< The socket handle associated with this endpoint */
  int wakefd; /**< An eventfd() handle for waking up an event loop when any client writes to a 'sub'
                   style server socket, this one handle collapses the collection of event handles
                   from many client sockets into one event handle. */
  bool started;  /**< True if the socket was successfully started (e.g. connect()
                   or bind() succeeded). */
  bool nonblock; /**< Set the socket to non-blocking mode */
  bool woke;     /**< Has the socket been woken up */
  client_nodes_head_t client_nodes_head; /**< The list of client nodes for a server socket */
  removed_nodes_head_t
    removed_nodes_head; /**< The list of client nodes that need to be removed and cleaned-up */
  int client_count;     /**< The number of clients for a server socket */
  pk_loop_t *loop;      /**< The event loop this endpoint is associated with */
  void *poll_handle;    /**< The poll handle for this socket */
  char path[PATH_MAX];  /**< The path to the socket */
};

static int create_un_socket(void);

static int bind_un_socket(int fd, const char *path);

static int start_un_listen(const char *path, int fd);

static int connect_un_socket(int fd, const char *path, bool retry_start);

static bool valid_socket_type_for_read(pk_endpoint_t *pk_ept);

static void teardown_client(client_context_t *ctx);

static int recv_impl(client_context_t *ctx, u8 *buffer, size_t *length_loc);

static int service_reads(client_context_t *ctx, pk_endpoint_receive_cb rx_cb, void *context);

static void send_close_socket_helper(client_context_t *ctx);

static int send_impl(client_context_t *ctx, const u8 *data, size_t length);

static void discard_read_data(client_context_t *ctx);

NESTED_FN_TYPEDEF(void,
                  foreach_client_fn_t,
                  pk_endpoint_t *pk_ept,
                  client_node_t *client_node,
                  void *context);

static void foreach_client(pk_endpoint_t *pk_ept, void *context, foreach_client_fn_t foreach_fn);

static void purge_client_node(client_node_t *client_node);

static void process_removed_clients(pk_endpoint_t *pk_ept);

static void record_disconnect(client_node_t *node);

static void handle_client_wake(pk_loop_t *loop, void *handle, int status, void *context);

static void accept_wake_handler(pk_loop_t *loop, void *handle, int status, void *context);

NESTED_FN_TYPEDEF(int, eintr_fn_t);

static bool retry_on_eintr(eintr_fn_t the_func, int priority, const char *error_message);

NESTED_FN_TYPEDEF(ssize_t, read_handler_fn_t, client_context_t *client_ctx, void *ctx);

static int read_and_receive_common(pk_endpoint_t *pk_ept,
                                   read_handler_fn_t read_handler,
                                   void *ctx);

static pk_endpoint_t *create_impl(const char *endpoint, pk_endpoint_type type, bool retry_start);

/**********************************************************************/
/************* pk_endpoint_create *************************************/
/**********************************************************************/

pk_endpoint_t *pk_endpoint_create(const char *endpoint, pk_endpoint_type type)
{
  return pk_endpoint_create_ex(endpoint, type, false);
}

pk_endpoint_t *pk_endpoint_create_ex(const char *endpoint, pk_endpoint_type type, bool retry)
{
  return create_impl(endpoint, type, retry);
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

  if (pk_ept->started && pk_ept->sock >= 0) {
    retry_on_eintr(NESTED_FN(int, (), { return shutdown(pk_ept->sock, SHUT_RDWR); }),
                   LOG_ERR,
                   "Failed to shutdown endpoint");
  }

  if (pk_ept->sock >= 0) {
    retry_on_eintr(NESTED_FN(int, (), { return close(pk_ept->sock); }),
                   LOG_ERR,
                   "Failed to close socket");
    pk_ept->sock = -1;
  }

  while (!LIST_EMPTY(&pk_ept->client_nodes_head)) {
    client_node_t *node = LIST_FIRST(&pk_ept->client_nodes_head);
    /* clang-tidy thinks this is a use-after-free for some reason? */
    purge_client_node(node); /* NOLINT */
  }

  if (pk_ept->wakefd >= 0) {
    retry_on_eintr(NESTED_FN(int, (), { return close(pk_ept->wakefd); }),
                   LOG_ERR,
                   "Failed to close eventfd");
    pk_ept->wakefd = -1;
  }

  if (pk_ept->poll_handle != NULL) {
    assert(pk_ept->loop != NULL);
    pk_loop_poll_remove(pk_ept->loop, pk_ept->poll_handle);
    pk_ept->poll_handle = NULL;
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
  if (count == 0) return 0;

  size_t length = count;

  read_handler_fn_t read_handler = NESTED_FN(ssize_t, (client_context_t * client_ctx, void *ctx), {
    size_t *length_loc = ctx;
    return recv_impl(client_ctx, buffer, length_loc);
  });

  int rc = read_and_receive_common(pk_ept, read_handler, &length);
  if (rc < 0) return rc;

  return length;
}

/**********************************************************************/
/************* pk_endpoint_receive ************************************/
/**********************************************************************/

int pk_endpoint_receive(pk_endpoint_t *pk_ept, pk_endpoint_receive_cb rx_cb, void *context)
{
  ASSERT_TRACE(pk_ept->nonblock);
  ASSERT_TRACE(rx_cb != NULL);

  read_handler_fn_t read_handler = NESTED_FN(ssize_t, (client_context_t * client_ctx, void *ctx), {
    service_reads(client_ctx, rx_cb, ctx);
    return 0;
  });

  return read_and_receive_common(pk_ept, read_handler, context);
}

/**********************************************************************/
/************* pk_endpoint_send ***************************************/
/**********************************************************************/

int pk_endpoint_send(pk_endpoint_t *pk_ept, const u8 *data, const size_t length)
{
  ASSERT_TRACE(pk_ept->type != PK_ENDPOINT_SUB && pk_ept->type != PK_ENDPOINT_SUB_SERVER);

  int rc = -1;

  if (pk_ept->type == PK_ENDPOINT_PUB || pk_ept->type == PK_ENDPOINT_REQ) {
    client_context_t ctx = (client_context_t){
      .ept = pk_ept,
      .handle = pk_ept->sock,
      .poll_handle = NULL,
      .node = NULL,
    };
    rc = send_impl(&ctx, data, length);
  } else if (pk_ept->type == PK_ENDPOINT_PUB_SERVER || pk_ept->type == PK_ENDPOINT_REP) {
    foreach_client(pk_ept,
                   &rc,
                   NESTED_FN(void, (pk_endpoint_t * pk_ept, client_node_t * node, void *ctx), {
                     /* TODO: Why are we saving rc here, invalid clients will just be removed... */
                     int *rc_loc = ctx;
                     *rc_loc = send_impl(&node->val, data, length);
                   }));
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
  int flags = fcntl(pk_ept->sock, F_GETFL, 0);

  if (flags < 0) {
    PK_LOG_ANNO(LOG_ERR, "fcntl error: %s", strerror(errno));
    return -1;
  }

  if (fcntl(pk_ept->sock, F_SETFL, flags | O_NONBLOCK) < 0) {
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

int pk_endpoint_loop_add(pk_endpoint_t *pk_ept, pk_loop_t *loop)
{
  ASSERT_TRACE(loop != NULL);

  /* The endpoint must be non-blocking to be associated with a loop */
  if (pk_endpoint_set_non_blocking(pk_ept) < 0) return -1;

  if (pk_ept->loop != NULL) {
    /* We do not support changing the loop assication (for now) */
    if (pk_ept->loop != loop) PK_LOG_ANNO(LOG_ERR, "cannot change associate loop");
    return pk_ept->loop == loop ? 0 : -1;
  }

  if (pk_ept->type == PK_ENDPOINT_SUB || pk_ept->type == PK_ENDPOINT_PUB
      || pk_ept->type == PK_ENDPOINT_REQ) {

    pk_ept->poll_handle = NULL;

    /* Later we may want to use the loop to do things like reconnecting? */
    pk_ept->loop = loop;

    return 0;
  }

  ASSERT_TRACE(pk_ept->loop == NULL);
  ASSERT_TRACE(pk_ept->poll_handle == NULL);

  pk_ept->loop = loop;
  pk_ept->poll_handle = pk_loop_poll_add(loop, pk_ept->sock, accept_wake_handler, pk_ept);

  return pk_ept->poll_handle != NULL ? 0 : -1;
}

/**************************************************************************/
/************* Helpers ****************************************************/
/**************************************************************************/

static int create_un_socket(void)
{
  int fd = -1;

  if ((fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
    PK_LOG_ANNO(LOG_ERR, "socket() error: %s", strerror(errno));
  }

  return fd;
}

static int bind_un_socket(int fd, const char *path)
{
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    PK_LOG_ANNO(LOG_ERR, "bind() error for socket: %s (path: %s)", strerror(errno), path);
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

static int connect_un_socket(int fd, const char *path, bool retry_start)
{
  struct sockaddr_un addr = {.sun_family = AF_UNIX};
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  int connect_errno = 0;
  int rc = 0;

  for (size_t retry = 0; retry <= CONNECT_RETRIES_MAX; retry++) {

    struct timespec ts = {.tv_nsec = MS_TO_NS(CONNECT_RETRY_SLEEP_MS)};
    if (retry > 0) {
      while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
        /* no-op, just need to retry nanosleep */;
      }
    }

    rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    connect_errno = errno;

    if (rc == 0 || !retry_start) break;
  }

  errno = connect_errno;
  return rc == 0 ? 0 : -1;
}

static bool valid_socket_type_for_read(pk_endpoint_t *pk_ept)
{
  if (pk_ept->type == PK_ENDPOINT_SUB) return true;

  if (pk_ept->type == PK_ENDPOINT_SUB_SERVER) return true;

  if (pk_ept->type == PK_ENDPOINT_REP) return true;

  if (pk_ept->type == PK_ENDPOINT_REQ) return true;

  return false;
}

static void teardown_client(client_context_t *ctx)
{
  if (ctx->ept->loop != NULL && ctx->poll_handle != NULL) {
    pk_loop_poll_remove(ctx->ept->loop, ctx->poll_handle);
    ctx->poll_handle = NULL;
  }

  ASSERT_TRACE(ctx->poll_handle == NULL);

  if (ctx->handle == -1) return;

  retry_on_eintr(NESTED_FN(int, (), { return shutdown(ctx->handle, SHUT_RDWR); }),
                 LOG_WARNING,
                 "Could not shutdown client socket");
  retry_on_eintr(NESTED_FN(int, (), { return close(ctx->handle); }),
                 LOG_WARNING,
                 "Coult not close client socket");

  ctx->handle = -1;
}

static int recv_impl(client_context_t *ctx, u8 *buffer, size_t *length_loc)
{
  ENDPOINT_DEBUG_LOG("handle: %d, ept: %p, poll_handle: %p, node: %p",
                     ctx->handle,
                     ctx->ept,
                     ctx->poll_handle,
                     ctx->node);

  int err = 0;
  int length = 0;

  struct iovec iov[1] = {0};
  struct msghdr msg = {0};

  iov[0].iov_base = buffer;
  iov[0].iov_len = *length_loc;

  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  while (1) {

    length = recvmsg(ctx->handle, &msg, 0);

    if (length >= 0) {
      if (length == 0) {
        ENDPOINT_DEBUG_LOG("socket closed");
        if (ctx->node != NULL) record_disconnect(ctx->node);
        teardown_client(ctx);
      }
      /* TODO: we should probably auto reconnect here if we're a PUB/SUB/REQ */
      /*   (non-server socket). */
      break;
    }

    if (errno == EINTR) {
      /* Retry if interrupted */
      ENDPOINT_DEBUG_LOG("got EINTR from recvmsg: %s", strerror(errno));
      continue;
    }

    if (ctx->ept->nonblock && errno == EAGAIN) {
      /* An "expected" error, don't need to report an error */
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

static int service_reads(client_context_t *ctx, pk_endpoint_receive_cb rx_cb, void *context)
{
  for (size_t i = 0; i < ENDPOINT_SERVICE_MAX; i++) {
    u8 buffer[READ_BUF_SIZE] = {0};
    size_t length = sizeof(buffer);
    int rc = recv_impl(ctx, buffer, &length);
    if (rc < 0) {
      if (rc == PKE_EAGAIN || rc == PKE_NOT_CONN) break;
      PK_LOG_ANNO(LOG_ERR, "failed to receive message");
      return -1;
    }
    if (length == 0) break;
    bool stop = rx_cb(buffer, length, context) != 0;
    if (stop) break;
  }
  return 0;
}

static void send_close_socket_helper(client_context_t *ctx)
{
  if (ctx->node != NULL) record_disconnect(ctx->node);
  teardown_client(ctx);
}

static int send_impl(client_context_t *ctx, const u8 *data, const size_t length)
{
  ENDPOINT_DEBUG_LOG("handle: %d, ept: %p, poll_handle: %p, node: %p",
                     ctx->handle,
                     ctx->ept,
                     ctx->poll_handle,
                     ctx->node);

  struct iovec iov[1] = {0};
  struct msghdr msg = {0};

  iov[0].iov_base = (u8 *)data;
  iov[0].iov_len = length;

  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  while (1) {

    int written = sendmsg(ctx->handle, &msg, 0);
    int error = errno;

    if (written != -1) {
      /* Break on success */
      ASSERT_TRACE(written == length);
      return 0;
    }

    if (error == EAGAIN || error == EWOULDBLOCK) {

      int queued_input = 0;
      int error = ioctl(ctx->handle, SIOCINQ, &queued_input);

      if (error < 0) PK_LOG_ANNO(LOG_WARNING, "unable to read SIOCINQ: %s", strerror(errno));

      int queued_output = 0;
      error = ioctl(ctx->handle, SIOCOUTQ, &queued_output);

      if (error < 0) PK_LOG_ANNO(LOG_WARNING, "unable to read SIOCOUTQ: %s", strerror(errno));

      PK_LOG_ANNO(LOG_WARNING,
                  "sendmsg returned EAGAIN, disconnecting and dropping %d queued bytes "
                  "(path: %s, node: %p)",
                  length + queued_input + queued_output,
                  ctx->ept->path,
                  ctx->node);

      send_close_socket_helper(ctx);

      return 0;
    }

    if (error == EINTR) {
      /* Retry if interrupted */
      ENDPOINT_DEBUG_LOG("sendmsg returned with EINTR");
      continue;
    }

    send_close_socket_helper(ctx);

    if (error != EPIPE && error != ECONNRESET) {
      PK_LOG_ANNO(LOG_ERR, "error in sendmsg: %s", strerror(error));
    }

    /* Return error */
    return -1;
  }
}

static void discard_read_data(client_context_t *ctx)
{
  u8 read_buf[READ_BUF_SIZE];
  size_t length = sizeof(read_buf);
  for (size_t count = 0; count < ENDPOINT_SERVICE_MAX; count++) {
    if (ctx == NULL || ctx->node == NULL) break;
    if (recv_impl(ctx, read_buf, &length) != 0) {
      break;
    }
  }
}

static void foreach_client(pk_endpoint_t *pk_ept, void *context, foreach_client_fn_t foreach_fn)
{
  client_node_t *node;
  LIST_FOREACH(node, &pk_ept->client_nodes_head, entries)
  {
    foreach_fn(pk_ept, node, context);
  }
  process_removed_clients(pk_ept);
}

static void purge_client_node(client_node_t *client_node)
{
  LIST_REMOVE(client_node, entries);
  teardown_client(&client_node->val);
  free(client_node);
}

static void process_removed_clients(pk_endpoint_t *pk_ept)
{
  while (!LIST_EMPTY(&pk_ept->removed_nodes_head)) {

    removed_node_t *removed_node = LIST_FIRST(&pk_ept->removed_nodes_head);
    LIST_REMOVE(removed_node, entries);

    ENDPOINT_DEBUG_LOG("removing client: client node: %d", removed_node->client_node);

    purge_client_node(removed_node->client_node);
    free(removed_node);
  }
}

static void record_disconnect(client_node_t *node)
{
  pk_endpoint_t *ept = node->val.ept;

  ASSERT_TRACE(ept != NULL);
  ASSERT_TRACE(node != NULL);

  --ept->client_count;

  removed_node_t *removed_node = malloc(sizeof(removed_node_t));
  *removed_node = (removed_node_t){.client_node = node};

  LIST_INSERT_HEAD(&ept->removed_nodes_head, removed_node, entries);

  if (ept->client_count < 0) {
    PK_LOG_ANNO(LOG_ERR | LOG_SBP, "client count is negative (count: %d)", ept->client_count);
  }
}

static void handle_client_wake(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)handle;

  client_context_t *client_context = (client_context_t *)context;

  if ((status & LOOP_ERROR) || (status & LOOP_DISCONNECTED)) {

    ENDPOINT_DEBUG_LOG("client disconnected: %s (%08x)", pk_loop_describe_status(status), status);

    teardown_client(client_context);
    record_disconnect(client_context->node);
    process_removed_clients(client_context->ept);

    return;
  }

  if ((status & LOOP_READ) && client_context->ept->type == PK_ENDPOINT_PUB_SERVER) {

    piksi_log(LOG_WARNING, "discarding read data from pub server");
    discard_read_data(client_context);

    return;
  }

  /* Don't wake-up loop again if one is already pending */
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

    retry_on_eintr(NESTED_FN(int, (), { return shutdown(clientfd, SHUT_RDWR); }),
                   LOG_WARNING,
                   "Could not shutdown client socket");
    retry_on_eintr(NESTED_FN(int, (), { return close(clientfd); }),
                   LOG_WARNING,
                   "Coult not close client socket");

    return;
  }

  LIST_INSERT_HEAD(&ept->client_nodes_head, client_node, entries);

  client_context_t *client_context = &client_node->val;

  ENDPOINT_DEBUG_LOG("new client_count: %d; path: %s", ept->client_count + 1, ept->path);

  int clientfd = pk_endpoint_accept(ept);

  ENDPOINT_DEBUG_LOG("new client_fd: %d", clientfd);

  client_context->handle = clientfd;
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

static bool retry_on_eintr(eintr_fn_t the_func, int priority, const char *error_message)
{
  while (the_func() != 0) {
    if (errno == EINTR) continue;
    piksi_log(priority, "%s: %s", error_message, pk_endpoint_strerror());
    return false;
  }
  return true;
}

static int read_and_receive_common(pk_endpoint_t *pk_ept, read_handler_fn_t read_handler, void *ctx)
{
  int rc = 0;

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

    foreach_client(pk_ept,
                   ctx,
                   NESTED_FN(void, (pk_endpoint_t * pk_ept, client_node_t * node, void *ctx), {
                     read_handler(&node->val, ctx);
                   }));

  } else {

    client_context_t client_ctx = (client_context_t){
      .ept = pk_ept,
      .handle = pk_ept->sock,
      .poll_handle = pk_ept->poll_handle,
      .node = NULL,
    };

    rc = read_handler(&client_ctx, ctx);
  }

  return rc;
}

static pk_endpoint_t *create_impl(const char *endpoint, pk_endpoint_type type, bool retry_start)
{
  ASSERT_TRACE(endpoint != NULL);

  pk_endpoint_t *pk_ept = (pk_endpoint_t *)malloc(sizeof(pk_endpoint_t));
  if (pk_ept == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate PK endpoint");
    goto failure;
  }

  *pk_ept = (pk_endpoint_t){
    .type = type,
    .sock = -1,
    .started = false,
    .nonblock = false,
    .woke = false,
    .wakefd = -1,
    .client_count = 0,
    .loop = NULL,
    .poll_handle = NULL,
  };

  strncpy(pk_ept->path, endpoint, sizeof(pk_ept->path));

  LIST_INIT(&pk_ept->client_nodes_head);
  LIST_INIT(&pk_ept->removed_nodes_head);

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
      piksi_log(LOG_ERR, "error creating PK SUB socket: %s", pk_endpoint_strerror());
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
  }

  size_t prefix_len = strstr(endpoint, IPC_PREFIX) != NULL ? strlen(IPC_PREFIX) : 0;

  if (do_bind) {
    int rc = unlink(endpoint + prefix_len);
    if (rc != 0 && errno != ENOENT) {
      PK_LOG_ANNO(LOG_WARNING, "unlink: %s", strerror(errno));
    }
  }

  int rc = do_bind ? bind_un_socket(pk_ept->sock, endpoint + prefix_len)
                   : connect_un_socket(pk_ept->sock, endpoint + prefix_len, retry_start);

  pk_ept->started = rc == 0;

  if (!pk_ept->started) {
    PK_LOG_ANNO(LOG_ERR,
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
