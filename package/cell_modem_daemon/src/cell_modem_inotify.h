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

bool cell_modem_tty_exists(const char* path);
void async_wait_for_tty(sbp_zmq_pubsub_ctx_t *pubsub_ctx);

#endif//_SWIFT_CELLMODEM_INOTIFY_H
