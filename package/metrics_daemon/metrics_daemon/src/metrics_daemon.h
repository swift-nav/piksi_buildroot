//
// Created by Yi Yang on 9/7/18.
//

#ifndef PIKSI_BUILDROOT_METRICS_DAEMON_H
#define PIKSI_BUILDROOT_METRICS_DAEMON_H
#ifdef __cplusplus
 extern "C" {
 #endif
int handle_walk_path(const char *fpath, const struct stat *sb, int tflag);
json_object *init_json_object(const char *path);
#ifdef __cplusplus
 }
 #endif
#endif // PIKSI_BUILDROOT_METRICS_DAEMON_H
