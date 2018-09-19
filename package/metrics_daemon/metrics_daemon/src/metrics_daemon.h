//
// Created by Yi Yang on 9/7/18.
//

#ifndef PIKSI_BUILDROOT_METRICS_DAEMON_H
#define PIKSI_BUILDROOT_METRICS_DAEMON_H

extern "C" {
int handle_walk_path(const char *fpath, const struct stat *sb, int tflag);
json_object *init_json_object(const char * path);
}
#endif //PIKSI_BUILDROOT_METRICS_DAEMON_H
