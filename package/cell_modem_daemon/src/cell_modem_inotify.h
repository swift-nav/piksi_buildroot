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

#ifndef _SWIFT_CELL_MODEM_INOTIFY_H
#define _SWIFT_CELL_MODEM_INOTIFY_H

typedef struct inotify_ctx_s inotify_ctx_t;
inotify_ctx_t * inotify_ctx_create(const char *path,
                                   int inotify_init_flags,
                                   uint32_t inotify_watch_flags,
                                   pk_loop_t *loop);
void inotify_ctx_destroy(inotify_ctx_t **ctx_loc);
bool cell_modem_tty_exists(const char* path);
int cell_modem_scan_for_modem(inotify_ctx_t *ctx);
void cell_modem_set_dev_to_invalid(inotify_ctx_t *ctx);
inotify_ctx_t * async_wait_for_tty(pk_loop_t *loop);

#endif//_SWIFT_CELL_MODEM_INOTIFY_H
