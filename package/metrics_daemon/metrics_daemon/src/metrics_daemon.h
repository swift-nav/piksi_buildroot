/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */
#ifndef PIKSI_BUILDROOT_METRICS_DAEMON_H
#define PIKSI_BUILDROOT_METRICS_DAEMON_H

#ifdef __cplusplus
extern "C" {
#endif

int handle_walk_path(const char *fpath, const struct stat *sb, int tflag);

void init_json_object(const char *path);

struct json_object *loop_through_folder_name(const char *process_path,
                                             const char *root,
                                             unsigned int root_len);

char *extract_filename(const char *str);
#ifdef __cplusplus
}
#endif

#endif // PIKSI_BUILDROOT_METRICS_DAEMON_H
