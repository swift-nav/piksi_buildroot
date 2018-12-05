/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swift-nav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_FIO_DEBUG_H
#define SWIFTNAV_FIO_DEBUG_H

extern bool fio_debug;

#define FIO_LOG_DEBUG(MsgPattern, ...)        \
  if (fio_debug) {                            \
    piksi_log(LOG_DEBUG,                      \
              ("%s: " MsgPattern " (%s:%d)"), \
              __FUNCTION__,                   \
              ##__VA_ARGS__,                  \
              __FILE__,                       \
              __LINE__);                      \
  }

#endif
