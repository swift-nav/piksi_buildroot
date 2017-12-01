/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

/* https://swiftnav.hackpad.com/SBP-Message-Whitelist-Design-toM7WBRqq3C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "whitelists.h"

/* Whitelist settings are kept as formatted strings of message ids and
 * rate dividers.  Strings are parsed in whilelist_notify()
 *
 * Examples:
 * ""
 *  - All messages are sent
 * "65535"
 *  - Only the heartbeat message is sent (message ID 65535)
 * "1234,5678/2,3456/10"
 *  - Message 1234 is sent at full rate
 *  - Message 5678 is sent at half rate
 *  - Message 3456 is sent at 1/10 rate
 */

enum port {
  PORT_UART0,
  PORT_UART1,
  PORT_USB0,
  PORT_TCP_SERVER0,
  PORT_TCP_SERVER1,
  PORT_TCP_CLIENT0,
  PORT_TCP_CLIENT1,
  PORT_UDP_SERVER0,
  PORT_UDP_SERVER1,
  PORT_UDP_CLIENT0,
  PORT_UDP_CLIENT1,
  PORT_MAX
};

typedef struct {
  const char *name;
  char wl[256];
} port_whitelist_config_t;

static port_whitelist_config_t port_whitelist_config[PORT_MAX] = {
  [PORT_UART0] = {
    .name = "uart0",
    .wl = "72,74,65535"
  },
  [PORT_UART1] = {
    .name = "uart1",
    .wl = "23,29,65,72,74,80,117,134,136,144,163,165,166,167,171,181,185,187,188,257,258,259,520,522,524,526,527,528,1025,2304,2305,65280,65282,65535"
    /*  This filter represents the messages in use by the console.
        It removes all ECEF nav messages as well as parts of nav msg.
        MsgThreadState             23
        MsgUartState               29
        MsgTrackingState           65
        MsgBasePosECEF             72
        MsgObs                     74
        MsgSpecan                  80
        MsgGloBiases              117        
        MsgEphemerisGPS           134
        MsgEphemerisGlo           136
        MsgIono                   144
        MsgFileioReadResp         163
        MsgSettingsReadResp       165
        MsgSettingsReadByIn       166
        MsgSettingsReadByIn       167
        MsgFileioWriteResp        171
        MsgDeviceMonitor          181
        MsgCommandResp            185
        MsgNetworkStateResp       187
        MsgCommandOutput          188
        MsgExtEvent               257
        MsgGPSTime                258
        MsgUtcTime                259
        MsgDops                   520
        MsgPosLLH                 522
        MsgBaselineNED            524
        MsgVelNED                 526
        MsgBaselineHeading        527
        MsgAgeCorrections         528
        MsgLog                   1025
        MsgImuRaw                2304
        MsgImuAux                2305
        MsgStartup              65280
        MsgDgnssStatus          65282
        MsgHeartbeat            65535 */
  },
  [PORT_USB0] = {
    .name = "usb0",
    .wl = ""
  },
  [PORT_TCP_SERVER0] = {
    .name = "tcp_server0",
    .wl = ""
  },
  [PORT_TCP_SERVER1] = {
    .name = "tcp_server1",
    .wl = ""
  },
  [PORT_TCP_CLIENT0] = {
    .name = "tcp_client0",
    .wl = ""
  },
  [PORT_TCP_CLIENT1] = {
    .name = "tcp_client1",
    .wl = ""
  },
  [PORT_UDP_SERVER0] = {
    .name = "udp_server0",
    .wl = ""
  },
  [PORT_UDP_SERVER1] = {
    .name = "udp_server1",
    .wl = ""
  },
  [PORT_UDP_CLIENT0] = {
    .name = "udp_client0",
    .wl = ""
  },
  [PORT_UDP_CLIENT1] = {
    .name = "udp_client1",
    .wl = ""
  }
};

int whitelist_notify(void *context)
{
  port_whitelist_config_t *port_whitelist_config =
      (port_whitelist_config_t *)context;

  char *c = port_whitelist_config->wl;
  unsigned tmp;
  enum {PARSE_ID, PARSE_AFTER_ID, PARSE_DIV, PARSE_AFTER_DIV} state = PARSE_ID;
  struct {
    unsigned id;
    unsigned div;
  } whitelist[128];
  int entries = 0;

  /* Simple parser for whitelist settings */
  while (*c) {
    switch (*c) {
    /* Integer token, this is an ID or divider */
    case '0' ... '9':
      tmp = strtoul(c, &c, 10);
      switch (state) {
      case PARSE_ID:
        state = PARSE_AFTER_ID;
        whitelist[entries].id = tmp;
        whitelist[entries].div = 1;
        entries++;
        break;
      case PARSE_DIV:
        state = PARSE_AFTER_DIV;
        whitelist[entries-1].div = tmp;
        break;
      default:
        return -1;
      }
      break;

    /* Divider token, following is divider */
    case '/':
      if (state == PARSE_AFTER_ID) {
        state = PARSE_DIV;
        c++;
      } else {
        return -1;
      }
      break;

    /* Separator token, following is message id */
    case ',':
      if ((state == PARSE_AFTER_ID) || (state == PARSE_AFTER_DIV)) {
        state = PARSE_ID;
        c++;
      } else {
        return -1;
      }
      break;

    /* Ignore whitespace */
    case ' ': case '\t': case '\n': case '\r': case '\v':
      c++;
      break;

    /* Invalid token, parse error */
    default:
      return -1;
    }
  }

  /* Parsed successfully, write config file and accept setting */
  char fn[256];
  sprintf(fn, "/etc/%s_filter_out_config", port_whitelist_config->name);
  FILE *cfg = fopen(fn, "w");
  for (int i = 0; i < entries; i++) {
    fprintf(cfg, "%x %x\n", whitelist[i].id, whitelist[i].div);
  }
  fclose(cfg);

  return 0;
}

int whitelists_init(settings_ctx_t *settings_ctx)
{
  for (int i = 0; i < PORT_MAX; i++) {
    settings_register(settings_ctx, port_whitelist_config[i].name, "enabled_sbp_messages",
                      port_whitelist_config[i].wl, sizeof(port_whitelist_config[i].wl),
                      SETTINGS_TYPE_STRING, whitelist_notify, &port_whitelist_config[i]);
  }
}
