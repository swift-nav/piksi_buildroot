/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <unistd.h>
#include <czmq.h>

struct async_child_ctx {
  zloop_t *loop;
  zmq_pollitem_t pollitem;
  int pid;
  int exit_status;

  void (*output_callback)(const char *buf, void *external_context);
  void (*exit_callback)(int status, void *external_context);
  void *external_context;

  struct async_child_ctx *next;
};

static struct async_child_ctx *async_children;

/* This pipe is used to synchronise reaping of the dead child from SIGCHLD
 * with the zmq event loop. */
static int wakepipe[2];
static zmq_pollitem_t wakeitem;
static zloop_t *wakeloop;

/* We use this unbuffered fgets function alternative so that the czmq loop
 * will keep calling us with output until we've read it all.
 */
static int raw_fgets(char *str, size_t len, int fd)
{
  size_t i;
  len--;
  for (i = 0; i < len; i++) {
    int r = read(fd, &str[i], 1);
    if ((r < 0) && (errno != EAGAIN))
      return r;
    if ((r <= 0) || (str[i] == '\n'))
      break;
  }
  str[i++] = 0;
  return i > 1 ? i : 0;
}

static int async_output_cb(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
  struct async_child_ctx *ctx = arg;
  char buf[128];

  if (raw_fgets(buf, sizeof(buf), item->fd) > 0) {
    if (ctx->output_callback)
      ctx->output_callback(buf, ctx->external_context);
  }

  return 0;
}

static int async_cleanup_dead_child(zloop_t *loop, zmq_pollitem_t *item, void *arg)
{
  struct async_child_ctx *ctx;
  read(item->fd, &ctx, sizeof(ctx));

  /* Child is dead, notify observer and clean up */
  ctx->exit_callback(ctx->exit_status, ctx->external_context);

  if (ctx == async_children) {
    async_children = ctx->next;
  } else {
    for (struct async_child_ctx *c = async_children; c; c = c->next) {
      if (c->next == ctx) {
        c->next = ctx->next;
        break;
      }
    }
  }
  free(ctx);

  return 0;
}

int async_spawn(zloop_t *loop, char **argv,
                void (*output_callback)(const char *buf, void *ctx),
                void (*exit_callback)(int status, void *ctx),
                void *external_context)
{
  assert(loop != NULL);
  if (!wakepipe[0]) {
    /* Set up the wake up pipe if not done already. */
    pipe(wakepipe);
    wakeitem.fd = wakepipe[0];
    wakeitem.events = ZMQ_POLLIN;
    zloop_poller(loop, &wakeitem, async_cleanup_dead_child, NULL);
    wakeloop = loop;
  }
  assert(loop == wakeloop);

  struct async_child_ctx *ctx = calloc(1, sizeof(*ctx));
  ctx->loop = loop;
  ctx->output_callback = output_callback;
  ctx->exit_callback = exit_callback;
  ctx->external_context = external_context;

  /* Open pipe to receive child's output */
  int pipefd[2];
  if (pipe(pipefd)) {
    perror("async_spawn pipe");
    free(ctx);
    return -1;
  }

  ctx->pid = fork();
  if (ctx->pid == 0) {
    /* Code executed by new child */
    close(STDIN_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    execvp(argv[0], argv);
    perror("async_spawn exec");
    exit(1);
  }

  close(pipefd[1]);

  if (ctx->pid == -1) {
    /* Failed to fork */
    perror("async_spawn fork");
    close(pipefd[0]);
    free(ctx);
    return -1;
  }

  ctx->pollitem.fd = pipefd[0];
  ctx->pollitem.events = ZMQ_POLLIN | ZMQ_POLLERR;
  int arg = O_NONBLOCK;
  fcntl(pipefd[0], F_SETFL, &arg);
  zloop_poller(loop, &ctx->pollitem, async_output_cb, ctx);

  /* Add to child list */
  ctx->next = async_children;
  async_children = ctx;
}

void async_child_waitpid_handler(pid_t pid, int status)
{
  /* Called by SIGCHLD handler with status from waitpid.
   * We save the status here, and will deal with it from the zmq loop by
   * writing to the wakeup pipe.
   */
  for (struct async_child_ctx *c = async_children; c; c = c->next) {
    if (c->pid == pid) {
      c->exit_status = status;
      c->pid = 0;
      /* Wake up the event loop */
      write(wakepipe[1], &c, sizeof(c));
      break;
    }
  }
}
