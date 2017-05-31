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

#ifndef __ASYNC_CHILD_H
#define __ASYNC_CHILD_H

#include <czmq.h>

int async_spawn(zloop_t *loop, char **argv,
                void (*output_callback)(const char *buf, void *ctx),
                void (*exit_callback)(int status, void *ctx),
                void *external_context);

void async_child_waitpid_handler(pid_t pid, int status);

#endif
