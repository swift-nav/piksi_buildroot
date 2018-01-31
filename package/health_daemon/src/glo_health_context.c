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


#include <stdlib.h>
#include <libsbp/sbp.h>

#include "glo_health_context.h"

/**
 * \brief private context for glo health state
 */
static struct glo_health_ctx_s {
  bool glonass_enabled;
  bool connected_to_base;
} glo_health_ctx = { .glonass_enabled = false, .connected_to_base = false };


void glo_context_receive_base_obs(void)
{
  glo_health_ctx.connected_to_base = true;
}

void glo_context_reset_connected_to_base(void)
{
  glo_health_ctx.connected_to_base = false;
}

bool glo_context_is_connected_to_base(void)
{
  return glo_health_ctx.connected_to_base;
}

void glo_context_set_glonass_enabled(bool enabled)
{
  glo_health_ctx.glonass_enabled = enabled;
}

bool glo_context_is_glonass_enabled(void)
{
  return glo_health_ctx.glonass_enabled;
}
