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

/**
 * @file    ota_settings.h
 * @brief   OTA settigns API.
 *
 * @defgroup    ota OTA
 * @addtogroup  ota
 * @{
 */

#ifndef SWIFTNAV_OTA_H
#define SWIFTNAV_OTA_H

#include <libpiksi/settings.h>

#define OTA_RUNIT_SERVICE_DIR "/var/run/ota_daemon/sv"
#define OTA_RUNIT_SERVICE_NAME "ota_daemon"

void ota_settings(settings_ctx_t *ctx);

#endif /* SWIFTNAV_OTA_H */
