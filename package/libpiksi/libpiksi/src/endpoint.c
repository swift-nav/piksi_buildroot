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
#include <execinfo.h>
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

#include <sys/ioctl.h>
#include <linux/sockios.h>

#include <libpiksi/logging.h>
#include <libpiksi/util.h>
#include <libpiksi/loop.h>

#include <libpiksi/endpoint.h>

// Maximum number of packets to service for one socket
#define ENDPOINT_SERVICE_MAX 32

#define IPC_PREFIX "ipc://"

#define MAX_CLIENTS 128

void print_trace (const char* assert_str, const char* file, const char* func, int lineno)
{
  void *array[32];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace(array, 32);
  strings = backtrace_symbols(array, size);

#define ASSERT_FAIL_MSG "ASSERT FAILURE: %s: %s (%s:%d), %zu backtrace entries:"

  piksi_log(LOG_ERR, ASSERT_FAIL_MSG, func, assert_str, file, lineno, size);
  for (i = 0; i < size; i++) {
     piksi_log(LOG_ERR, "backtrace: %s", strings[i]);
  }

  fprintf(stderr, ASSERT_FAIL_MSG "\n", func, assert_str, file, lineno, size);
  for (i = 0; i < size; i++) {
     fprintf(stderr, "backtrace: %s\n", strings[i]);
  }

  free(strings);
}

#define STRINGIFY(a) _STRINGIFY(a)
#define _STRINGIFY(a) #a

#define ASSERT_TRACE(TheAssert) \
  { if(!(TheAssert)) { print_trace(STRINGIFY(TheAssert), __FILE__,\
    __FUNCTION__, __LINE__); exit(EXIT_FAILURE); } }

typedef struct {
  pk_endpoint_t *ept;
  int fd;
  bool valid;
  int slot;
  void *poll_handle;
} client_context_t;

struct pk_endpoint_s {
  pk_endpoint_type type;
  int sock;
  int wakefd;
  int eid;
  bool nonblock;
  bool woke;
  client_context_t clients[MAX_CLIENTS];
  int client_count;
  pk_loop_t *loop;
  void *poll_handle;
  void *poll_handle_read;
  char path[PATH_MAX];
};


static int index_of_client_slot(pk_endpoint_t *pk_ept, client_context_t *ctx);
static int find_free_client_slot(pk_endpoint_t *pk_ept);
static void record_disconnect(pk_endpoint_t *pk_ept, int slot);
static void invalidate_client_slot(pk_endpoint_t *pk_ept, int slot);

static void accept_wake_handler(pk_loop_t *loop, void *handle, int status, void *context);

static int create_un_socket() {

  int fd = -1;

  if ( (fd = socket(AF_UNIX, SOCK_SEQPACKET, 0)) == -1) {
    pk_log_anno(LOG_ERR, "socket error: %s", strerror(errno));
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

static int start_un_listen(const char* path, int fd) {

  if (listen(fd, 50) == -1) {

    pk_log_anno(LOG_ERR, "listen() error for socket: %s (path: %s)", strerror(errno), path);
    return -1;
  }

  return 0;
}

static int connect_un_socket(int fd, const char* path) {

  struct sockaddr_un addr = { .sun_family = AF_UNIX };
  strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);

  if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    pk_log_anno(LOG_ERR, "connect error: %s (path: %s)", strerror(errno), path);
    return -1;
  }

  return 0;
}

pk_endpoint_t * pk_endpoint_create(const char *endpoint, pk_endpoint_type type)
{
  ASSERT_TRACE( endpoint != NULL );

  pk_endpoint_t *pk_ept = (pk_endpoint_t *)malloc(sizeof(pk_endpoint_t));
  if (pk_ept == NULL) {
    piksi_log(LOG_ERR, "Failed to allocate PK endpoint");
    goto failure;
  }

  pk_ept->type = type;
  pk_ept->sock = -1;
  pk_ept->eid = -1;
  pk_ept->nonblock = false;
  pk_ept->woke = false;
  pk_ept->client_count = 0;
  pk_ept->loop = NULL;
  pk_ept->poll_handle = NULL;
  pk_ept->poll_handle_read = NULL;
  strcpy(pk_ept->path, endpoint);

  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    invalidate_client_slot(pk_ept, i);
  }

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
      piksi_log(LOG_ERR, "error creating PK PUB/SUB socket: %s",
                         pk_endpoint_strerror());
      goto failure;
    }
  } break;
  case PK_ENDPOINT_REP:
    do_bind = true;
  case PK_ENDPOINT_REQ:
  {
    pk_ept->sock = create_un_socket();
    if (pk_ept->sock < 0) {
      piksi_log(LOG_ERR, "error creating PK REQ/REP socket: %s",
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

  size_t prefix_len = strstr(endpoint, IPC_PREFIX) != NULL ?  strlen(IPC_PREFIX) : 0;

  if (do_bind) {
    int rc = unlink(endpoint + prefix_len);
    if (rc != 0 && errno != ENOENT) {
      pk_log_anno(LOG_WARNING, "unlink: %s", strerror(errno));
    }
  }

  pk_ept->eid = do_bind ? bind_un_socket(pk_ept->sock, endpoint+prefix_len)
                        : connect_un_socket(pk_ept->sock, endpoint+prefix_len);

  if (pk_ept->eid < 0) {
    piksi_log(LOG_ERR, "Failed to %s socket: %s",
                       do_bind ? "bind" : "connect",
                       pk_endpoint_strerror());
    goto failure;
  }

  if (do_bind) {

    if (start_un_listen(endpoint, pk_ept->sock) < 0) goto failure;

    int rc = chmod(endpoint+prefix_len, 0777);
    if (rc != 0) {
      pk_log_anno(LOG_WARNING, "chmod: %s", strerror(errno));
    }

    pk_ept->wakefd = eventfd(0, EFD_NONBLOCK);

    if (pk_ept->wakefd < 0) {
      pk_log_anno(LOG_ERR, "eventfd: %s", strerror(errno));
      goto failure;
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
  ASSERT_TRACE( pk_ept != NULL );

  /* Pub and pub server are send only, they should not need to be polled for
   * input, therefore this function should not be called on these socket
   * types.
   */
  if (pk_ept->type == PK_ENDPOINT_PUB || pk_ept->type == PK_ENDPOINT_PUB_SERVER) {
    return -1;
  }

  if (pk_ept->type == PK_ENDPOINT_SUB || pk_ept->type == PK_ENDPOINT_REQ) {
    return pk_ept->sock;
  }

  if (pk_ept->type == PK_ENDPOINT_SUB_SERVER || pk_ept->type == PK_ENDPOINT_REP) {
    return pk_ept->wakefd;
  }
}

static int recv_impl(pk_endpoint_t *ept,
                     int sock,
                     u8 *buffer,
                     size_t *length_loc,
                     bool nonblocking,
                     pk_loop_t *loop,
                     void *poll_handle,
                     int slot)
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
//    pk_log_anno(LOG_DEBUG, "recvmsg: length: %d", length);

    if (length >= 0) {
      if (length == 0) {
        pk_log_anno(LOG_DEBUG, "socket closed");
        if (loop != NULL) {
          ASSERT_TRACE( poll_handle != NULL );
          pk_loop_poll_remove(loop, poll_handle);
        }
        if (slot >= 0) record_disconnect(ept, slot);
        shutdown(sock, SHUT_RDWR);
        close(sock);
      }
      // TODO: we should probably auto reconnect here if we're a PUB/SUB/REQ
      //   (non-server socket).
      break;

    } else if (errno == EINTR) {
      /* Retry if interrupted */
      pk_log_anno(LOG_DEBUG, "got EINTR from recvmsg: %s", strerror(errno));
      continue;

    } else if (nonblocking && errno == EAGAIN) {
      // An "expected" error, don't need to report an error
      return -1;

    } else {
      /* Return error */
      pk_log_anno(LOG_ERR, "recvmsg error: %s", strerror(errno));
      return -1;
    }
  }

  *length_loc = (size_t) length;

  return 0;
}

ssize_t pk_endpoint_read(pk_endpoint_t *pk_ept, u8 *buffer, size_t count)
{
  ASSERT_TRACE( pk_ept != NULL );
  ASSERT_TRACE( pk_ept->type != PK_ENDPOINT_PUB || pk_ept->type != PK_ENDPOINT_PUB_SERVER );

  ASSERT_TRACE( buffer != NULL );
  ASSERT_TRACE( count > 0 );

  size_t length = count;
  if (recv_impl(pk_ept, pk_ept->sock, buffer, &length, pk_ept->nonblock, NULL, NULL, -1) != 0) {
    piksi_log(LOG_ERR, "failed to receive message");
    return -1;
  }

  return length;
}

static int service_reads(pk_endpoint_t *ept,
                         int fd,
                         pk_loop_t *loop,
                         int client_slot,
                         void *poll_handle,
                         pk_endpoint_receive_cb rx_cb,
                         void *context)
{
  for (size_t i = 0; i < ENDPOINT_SERVICE_MAX; i++) {
    u8 buffer[4096];
    size_t length = sizeof(buffer);
    if (recv_impl(ept, fd, buffer, &length, true, loop, poll_handle, client_slot) != 0) {
      if (errno == EWOULDBLOCK) break;
      piksi_log(LOG_ERR, "failed to receive message");
      return -1;
    }
    if (length == 0) break;
    bool stop = rx_cb(buffer, length, context) != 0;
    if (stop) break;
  }
  return 0;
}

int pk_endpoint_receive(pk_endpoint_t *pk_ept, pk_endpoint_receive_cb rx_cb, void *context)
{
  ASSERT_TRACE( pk_ept != NULL );
  ASSERT_TRACE( pk_ept->type != PK_ENDPOINT_PUB || pk_ept->type != PK_ENDPOINT_PUB_SERVER );
  ASSERT_TRACE( pk_ept->nonblock );
  ASSERT_TRACE( rx_cb != NULL );

  if (pk_ept->type == PK_ENDPOINT_SUB_SERVER) {

    int64_t counter = 0;
    ssize_t c = read(pk_ept->wakefd, &counter, sizeof(counter));

    if (c < 0 || c != sizeof(counter)) {
      pk_log_anno(LOG_ERR, "invalid read size from eventfd: %zd", c);
      return -1;
    }

    ASSERT_TRACE( counter == 1 );
    ASSERT_TRACE( pk_ept->woke );

    pk_ept->woke = false;

    for (size_t client_slot = 0; client_slot < MAX_CLIENTS; client_slot++) {

      if (!pk_ept->clients[client_slot].valid)
        continue;

      service_reads(pk_ept,
                    pk_ept->clients[client_slot].fd,
                    pk_ept->loop,
                    client_slot,
                    pk_ept->clients[client_slot].poll_handle,
                    rx_cb,
                    context);
    }

  } else {
    service_reads(pk_ept, pk_ept->sock, pk_ept->loop, -1, pk_ept->poll_handle, rx_cb, context);
  }

  return 0;
}

static int send_impl(pk_endpoint_t *ept, int sock, const u8 *data, const size_t length, pk_loop_t *loop, void* poll_handle, int slot)
{
  struct iovec iov[1] = {0};
  struct msghdr msg = {0};

  iov[0].iov_base = (u8*) data;
  iov[0].iov_len  = length;

  msg.msg_iov     = iov;
  msg.msg_iovlen  = 1;

  void close_socket_helper() {
      if (loop != NULL) {
        ASSERT_TRACE( poll_handle != NULL );
        ASSERT_TRACE( slot >= 0 );
        pk_loop_poll_remove(loop, poll_handle);
        record_disconnect(ept, slot);
      }
      shutdown(sock, SHUT_RDWR);
      close(sock);
  };

  while (1) {

    int written = sendmsg(sock, &msg, 0);
    int error = errno;

//    pk_log_anno(LOG_DEBUG, "sendmsg: length: %d", length);

    if (written != -1) {
      /* Break on success */
      ASSERT_TRACE( written == length );
      return 0;

    } else if (error == EAGAIN || error == EWOULDBLOCK) {

      int queued_input = -1;
      int error = ioctl(sock, SIOCINQ, &queued_input);

      if (error < 0) pk_log_anno(LOG_DEBUG, "unable to read SIOCINQ: %s", strerror(errno));

      int queued_output = -1;
      error = ioctl(sock, SIOCOUTQ, &queued_output);

      if (error < 0) pk_log_anno(LOG_DEBUG, "unable to read SIOCOUTQ: %s", strerror(errno));

      pk_log_anno(LOG_WARNING, "sendmsg returned EAGAIN, dropping %d bytes (path: %s, slot: %d, queued input: %d, queued output: %d)", length, ept->path, slot, queued_input, queued_output);

      close_socket_helper();

      return 0;

    } else if (error == EINTR) {
      /* Retry if interrupted */
      pk_log_anno(LOG_DEBUG, "sendmsg returned with EINTR");
      continue;

    } else {

      close_socket_helper();

      if (error != EPIPE && error != ECONNRESET) {
        /* Return error */
        pk_log_anno(LOG_ERR, "error in sendmsg: %s", strerror(error));
      }

      return -1;
    }
  }
}

int pk_endpoint_send(pk_endpoint_t *pk_ept, const u8 *data, const size_t length)
{
  ASSERT_TRACE( pk_ept != NULL );
  ASSERT_TRACE( pk_ept->type != PK_ENDPOINT_SUB || pk_ept->type != PK_ENDPOINT_SUB_SERVER );

  int rc = -1;

  if (pk_ept->type == PK_ENDPOINT_PUB) {
    rc = send_impl(pk_ept, pk_ept->sock, data, length, NULL, NULL, -1);
  } else if (pk_ept->type == PK_ENDPOINT_PUB_SERVER || pk_ept->type == PK_ENDPOINT_REP) {
    for (int idx = 0; idx < MAX_CLIENTS; idx++) {
      if (!pk_ept->clients[idx].valid) continue;
      rc = send_impl(pk_ept, pk_ept->clients[idx].fd, data, length, pk_ept->loop, pk_ept->clients[idx].poll_handle, idx);
    }
  }

  return rc;
}

const char * pk_endpoint_strerror(void)
{
  return strerror(errno);
}

int pk_endpoint_set_non_blocking(pk_endpoint_t *pk_ept)
{
  int status = -1;

  if ( (status = fcntl(pk_ept->sock, F_SETFL, fcntl(pk_ept->sock, F_GETFL, 0) | O_NONBLOCK)) ) {
    pk_log_anno(LOG_ERR, "fcntl error: %s", strerror(errno));
    return -1;
  }

  pk_ept->nonblock = true;

  return 0;
}

int pk_endpoint_accept(pk_endpoint_t *pk_ept)
{
  int cl;

  if ( (cl = accept(pk_ept->sock, NULL, NULL)) == -1) {
    pk_log_anno(LOG_ERR, "accept error: %s", strerror(errno));
    return -1;
  }

  return cl;
}

static void discard_read_data(pk_loop_t *loop, client_context_t *ctx)
{
  u8 read_buf[4096];
  size_t length = sizeof(read_buf);

  for (int count = 0; count < ENDPOINT_SERVICE_MAX; count++) {
    if (recv_impl(ctx->ept, ctx->fd, read_buf, &length, true, loop, ctx->poll_handle, ctx->slot) != 0) {
      break;
    }
  }
}

static void record_disconnect(pk_endpoint_t *ept, int slot)
{
  ASSERT_TRACE( ept != NULL );
  ASSERT_TRACE( slot >= 0 );
  ASSERT_TRACE( slot < MAX_CLIENTS );

  --ept->client_count;
  invalidate_client_slot(ept, slot);
}

static void handle_client_wake(pk_loop_t *loop, void *handle, int status, void *context)
{
  client_context_t *client_context = (client_context_t *)context;

  ASSERT_TRACE( client_context->valid );

  if (status == LOOP_ERROR || status == LOOP_DISCONNECTED) {

    pk_log_anno(LOG_DEBUG, "client disconnected: %s", pk_loop_describe_status(status));

    close(client_context->fd);
    record_disconnect(client_context->ept, client_context->slot);

    return;
  }

  if (status == LOOP_READ && client_context->ept->type == PK_ENDPOINT_PUB_SERVER) {

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
  ASSERT_TRACE( loop != NULL );

  if (status != LOOP_SUCCESS) {

    if (status == LOOP_ERROR) {
      pk_log_anno(LOG_ERR, "status: %s; error: %s",
                  pk_loop_describe_status(status), pk_loop_last_error(loop));
    } else {
      pk_log_anno(LOG_ERR, "status: %s", pk_loop_describe_status(status));
    }

    return;
  }

  pk_endpoint_t *ept = (pk_endpoint_t *)context;
  ASSERT_TRACE( ept != NULL );

  int client_slot = find_free_client_slot(ept);

  if (client_slot < 0) {

    piksi_log(LOG_WARNING, "unable to add new client, closing connection");
    int clientfd = pk_endpoint_accept(ept);

    shutdown(clientfd, SHUT_RDWR);
    close(clientfd);

    return;
  }

  pk_log_anno(LOG_DEBUG, "new client_slot: %d", client_slot);

  client_context_t *client_context = &ept->clients[client_slot];

  pk_log_anno(LOG_DEBUG, "new client_count: %d; path: %s",
              ept->client_count+1, ept->path);

  int clientfd = pk_endpoint_accept(ept);

  pk_log_anno(LOG_DEBUG, "new client_fd: %d", clientfd);

  client_context->fd = clientfd;
  client_context->ept = ept;
  client_context->valid = true;
  client_context->slot = client_slot;

  if (fcntl(clientfd, F_SETFL, fcntl(clientfd, F_GETFL, 0) | O_NONBLOCK) < 0) {
    pk_log_anno(LOG_WARNING, "fcntl error: %s", strerror(errno));
  }

  ++ept->client_count;

  client_context->poll_handle =
    pk_loop_poll_add(loop, clientfd, handle_client_wake, client_context);

  ASSERT_TRACE( client_context->poll_handle != NULL );
}

int pk_endpoint_loop_add(pk_endpoint_t *pk_ept, pk_loop_t *loop, void *poll_handle_read)
{
  pk_log_anno(LOG_DEBUG, "loop: %p; pk_ept->loop: %p", loop, pk_ept->loop);

  if (pk_endpoint_set_non_blocking(pk_ept) < 0) return -1;

  if (pk_ept->type == PK_ENDPOINT_SUB ||
      pk_ept->type == PK_ENDPOINT_PUB ||
      pk_ept->type == PK_ENDPOINT_REQ)
  {
    ASSERT_TRACE(poll_handle_read != NULL);
    pk_ept->poll_handle == poll_handle_read;

    return 0;
  }

  ASSERT_TRACE( loop != NULL );

  if (pk_ept->loop == loop) {

    /* If we're a server style PUB/SUB socket, pk_loop_endpoint_reader_add will
     * pass a poll_handle to NULL because some else is handling the read events
     * for the socket
     */

    ASSERT_TRACE( poll_handle_read != NULL );
    ASSERT_TRACE( pk_ept->poll_handle_read == NULL );

    ASSERT_TRACE( pk_ept->type == PK_ENDPOINT_SUB_SERVER || pk_ept->type == PK_ENDPOINT_REP );

    pk_ept->poll_handle_read = poll_handle_read;

    return 0;
  }

  ASSERT_TRACE( pk_ept->loop == NULL );
  ASSERT_TRACE( pk_ept->poll_handle == NULL );

  if (pk_endpoint_set_non_blocking(pk_ept) < 0)
    return -1;

  pk_ept->loop = loop;

  ASSERT_TRACE( poll_handle_read == NULL );
  ASSERT_TRACE( pk_ept->poll_handle == NULL );

  pk_ept->poll_handle =
    pk_loop_poll_add(loop, pk_ept->sock, accept_wake_handler, pk_ept);

  return pk_ept->poll_handle != NULL ? 0 : -1;
}

int find_free_client_slot(pk_endpoint_t *pk_ept)
{
  int slot = -1;
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (!pk_ept->clients[i].valid) {
      slot = (int) i;
      break;
    }
  }
  if (slot < 0) pk_log_anno(LOG_ERR, "no free client slots");
  return slot;
}

int index_of_client_slot(pk_endpoint_t *pk_ept, client_context_t *ctx)
{
  int slot = -1;
  for (size_t i = 0; i < MAX_CLIENTS; i++) {
    if (ctx->fd == pk_ept->clients[i].fd) {
      slot = (int) i;
      break;
    }
  }
  if (slot < 0) pk_log_anno(LOG_ERR, "could not find slot for given client context");
  return slot;
}

static void invalidate_client_slot(pk_endpoint_t *pk_ept, int slot)
{
  ASSERT_TRACE( slot < MAX_CLIENTS );

  pk_ept->clients[slot] = (client_context_t) {
    .fd = -1,
    .valid = false,
    .ept = NULL,
    .slot = -1,
    .poll_handle = NULL,
  };
}
