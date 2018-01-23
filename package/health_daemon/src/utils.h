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

#ifndef __HEALTH_MONITOR_UTILS_H
#define __HEALTH_MONITOR_UTILS_H

#define SETTING_SECTION_ACQUISITION "acquisition"
#define SETTING_GLONASS_ACQUISITION_ENABLED "glonass_acquisition_enabled"

int health_util_parse_setting_read_resp(const u8 *msg,
                                        u8 msg_n,
                                        const char **section,
                                        const char **name,
                                        const char **value);

int health_util_check_glonass_enabled(const char *section,
                                      const char *name,
                                      const char *value,
                                      bool *result);

#endif /* __HEALTH_MONITOR_UTILS_H */
