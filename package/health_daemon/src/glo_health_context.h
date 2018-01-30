/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 *  be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __HEALTH_MONITOR_GLO_CONTEXT_H
#define __HEALTH_MONITOR_GLO_CONTEXT_H

void glo_context_receive_base_obs(void);
void glo_context_reset_connected_to_base(void);
bool glo_context_is_connected_to_base(void);
void glo_context_set_glonass_enabled(bool enabled);
bool glo_context_is_glonass_enabled(void);

#endif /* __HEALTH_MONITOR_GLO_CONTEXT_H */
