/*
 * Copyright (C) 2011-2017 Swift Navigation Inc.
 * Contact: Johannes Walter <johannes@swift-nav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef SWIFTNAV_NAP_REGS_H
#define SWIFTNAV_NAP_REGS_H

//#include <libswiftnav/config.h>

/* Include register maps */
//#include "registers/nt1065_frontend.h"
#include "registers_swiftnap.h"

#define NAP_TRK_SPACING_CHIPS_Pos (6U)
#define NAP_TRK_SPACING_CHIPS_Msk (0x7U)

#define NAP_TRK_SPACING_SAMPLES_Pos (0U)
#define NAP_TRK_SPACING_SAMPLES_Msk (0x3FU)

/* Instances */
#define NAP ((swiftnap_t *)0x43C00000)
#define NAP_FE ((nt1065_frontend_t *)0x43C10000)

#endif /* SWIFTNAV_NAP_REGS_H */
