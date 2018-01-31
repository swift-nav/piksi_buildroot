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

/**
 * \brief glo_context_receive_base_obs - indicate base obs received
 */
void glo_context_receive_base_obs(void);

/**
 * \brief glo_context_reset_connected_to_base - indicate base is disconnected
 */
void glo_context_reset_connected_to_base(void);

/**
 * \brief glo_context_is_connected_to_base - evaluate base connection state
 * \return true if base is currently connected and receiving obs, otherwise false
 */
bool glo_context_is_connected_to_base(void);

/**
 * \brief glo_context_set_glonass_enabled - set the current glonass enable state
 * \param enabled: bool indicating if glonass enable is set to true or false
 */
void glo_context_set_glonass_enabled(bool enabled);

/**
 * \brief glo_context_is_glonass_enabled - evaluate glonass enabled state
 * \return true if glonass_aquisition_enabled is true, otherwise false
 */
bool glo_context_is_glonass_enabled(void);

#endif /* __HEALTH_MONITOR_GLO_CONTEXT_H */
