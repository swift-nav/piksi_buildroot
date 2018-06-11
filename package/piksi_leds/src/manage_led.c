/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <libsbp/common.h>
#include <libpiksi/logging.h>

#include "led_adp8866.h"
#include "firmware_state.h"

#define LED_POS_R 0
#define LED_POS_G 1
#define LED_POS_B 2
#define LED_LINK_R 3
#define LED_LINK_G 4
#define LED_LINK_B 5
#define LED_MODE_R 6
#define LED_MODE_G 7
#define LED_MODE_B 8

#define LED_MAX LED_ADP8866_BRIGHTNESS_MAX

/* LED driver uses a square law output encoding. Calculate RGB components
   such that total current (and thus brightness) is constant. */
#define RGB_TO_RGB_LED_COMPONENT(_r, _g, _b, _c) \
  (LED_MAX * sqrtf((float)(_c) / ((_r) + (_g) + (_b))))

#define RGB_TO_RGB_LED(_r, _g, _b)                 \
  (((_r) == 0) && ((_g) == 0) && ((_b) == 0))      \
      ? (rgb_led_state_t){.r = 0, .g = 0, .b = 0}  \
      : (rgb_led_state_t) {                        \
    .r = RGB_TO_RGB_LED_COMPONENT(_r, _g, _b, _r), \
    .g = RGB_TO_RGB_LED_COMPONENT(_r, _g, _b, _g), \
    .b = RGB_TO_RGB_LED_COMPONENT(_r, _g, _b, _b)  \
  }

#define LED_COLOR_OFF RGB_TO_RGB_LED(0, 0, 0)
#define LED_COLOR_RED RGB_TO_RGB_LED(255, 0, 0)
#define LED_COLOR_BLUE RGB_TO_RGB_LED(0, 0, 255)
#define LED_COLOR_ORANGE RGB_TO_RGB_LED(255, 131, 0)
#define LED_COLOR_PURPLE RGB_TO_RGB_LED(128, 0, 128)

#define MANAGE_LED_THREAD_PERIOD_MS 10
#define SLOW_BLINK_PERIOD_MS 500
#define FAST_BLINK_PERIOD_MS 250

#define LED_MODE_TIMEOUT_MS 1500

typedef struct {
  u8 r;
  u8 g;
  u8 b;
} rgb_led_state_t;

typedef enum { LED_OFF, LED_BLINK_SLOW, LED_BLINK_FAST, LED_ON } blink_mode_t;

typedef enum {
  POS_NO_SIGNAL,
  POS_ANTENNA,
  POS_TRK_AT_LEAST_FOUR,
  POS_GNSS,
  POS_INS_DEAD_RECK,
  POS_INS_GNSS
} pos_state_t;

typedef enum { LINK_NO_NETWORK, LINK_NETWORK_AVAILABLE } link_state_t;

typedef enum { OBS_NO_EVENT, OBS_EVENT } obs_state_t;

typedef enum { MODE_NO_RTK, MODE_RTK_FLOAT, MODE_RTK_FIXED } mode_state_t;

typedef struct device_state_s {
  pos_state_t pos_state;
  link_state_t link_state;
  obs_state_t obs_state;
  mode_state_t mode_state;
} device_state_t;

typedef struct {
  blink_mode_t mode;            /* Current mode */
  struct timespec state_change; /* Time of previous on/off event */
  bool on_off;                  /* Current state */
} blinker_state_t;

static int network_available_fd;
static const char *network_available;

static u32 elapsed_ms(const struct timespec *then) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec * 1000 + now.tv_nsec / 1000000) -
         (then->tv_sec * 1000 + then->tv_nsec / 1000000);
}

/** Update LED blink state.
 *
 * \param[in] b            LED state information.
 *
 * \return bool, true for ON, false for OFF.
 */
static bool blinker_update(blinker_state_t *b) {
  u16 period_ms;

  switch (b->mode) {
    case LED_OFF:
      return false;
      break;
    case LED_ON:
      return true;
      break;
    case LED_BLINK_SLOW:
      period_ms = SLOW_BLINK_PERIOD_MS;
      break;
    case LED_BLINK_FAST:
      period_ms = FAST_BLINK_PERIOD_MS;
      break;
    default:
      assert(!"Unknown mode.");
      break;
  }

  u32 elapsed = elapsed_ms(&b->state_change);

  if (elapsed >= period_ms) {
    b->on_off = !b->on_off;
    clock_gettime(CLOCK_MONOTONIC, &b->state_change);
  }

  return b->on_off;
}

/** Update POS LED state with invalid spp
 *  Used when spp state from firmware is not valid,
 *  this is a helper function for update_pos_state
 * \param[out] dev_state: Device state reference
 * \param[in] state: Firmware solution state
 */
static void update_pos_state_invalid_spp(device_state_t *dev_state,
                                         struct soln_state *state)
{
  if (state->sats >= 4) {
    dev_state->pos_state = POS_TRK_AT_LEAST_FOUR;
  } else if (state->antenna) {
    dev_state->pos_state = POS_ANTENNA;
  } else {
    dev_state->pos_state = POS_NO_SIGNAL;
  }
}

/** Update POS LED state
 *  Will set dev_state with the current state for the POS LED
 * \param[out] dev_state: Device state reference
 * \param[in] state: Firmware solution state
 */
static void update_pos_state(device_state_t *dev_state,
                             struct soln_state *state)
{
  u32 elapsed = elapsed_ms(&state->spp.systime);

  if (elapsed >= LED_MODE_TIMEOUT_MS) {
    update_pos_state_invalid_spp(dev_state, state);
  } else {
    switch (state->spp.mode) {
    case SPP_MODE_DEAD_RECK:
    {
      if (state->spp.ins_mode == INS_MODE_NONE) {
        //piksi_log(LOG_ERR,
        //          "POS LED State Error: spp dead reckoning without ins used.");
      }
      dev_state->pos_state = POS_INS_DEAD_RECK;
    } break;
    case SPP_MODE_FIXED:
    case SPP_MODE_FLOAT:
    case SPP_MODE_DGNSS:
    case SPP_MODE_SBAS:
    case SPP_MODE_SPP:
    {
      if (state->spp.ins_mode == INS_MODE_INS_USED) {
        dev_state->pos_state = POS_INS_GNSS;
      } else {
        dev_state->pos_state = POS_GNSS;
      }
    } break;
    case SPP_MODE_INVALID:
    {
      update_pos_state_invalid_spp(dev_state, state);
    } break;
    default:
    {
      //piksi_log(LOG_ERR,
      //          "POS LED State Error: Unknown mode %d.", state->spp.mode);
      dev_state->pos_state = POS_NO_SIGNAL;
    } break;
    }
  }
}

/** Update LED state
 *  Will set dev_state with the current state for the LINK LED
 * \param[out] dev_state: Device state reference
 */
static void update_link_state(device_state_t *dev_state) {
  if (network_available && (*network_available == '1')) {
    dev_state->link_state = LINK_NETWORK_AVAILABLE;
  } else {
    dev_state->link_state = LINK_NO_NETWORK;
  }
}

/** Update LED state
 *  Will set dev_state obs event state
 * \param[out] dev_state: Device state reference
 * \param[in] base_obs_msg_counter: current count of base observations received
 */
static void update_obs_state(device_state_t *dev_state, u8 base_obs_msg_counter) {
  static u8 last_base_obs_msg_counter = 0;

  if (base_obs_msg_counter != last_base_obs_msg_counter) {
    last_base_obs_msg_counter = base_obs_msg_counter;
    dev_state->obs_state = OBS_EVENT;
  } else {
    dev_state->obs_state = OBS_NO_EVENT;
  }
}

/** Update LED state
 *  Will set dev_state with the current state for the MODE LED
 * \param[out] dev_state: Device state reference
 * \param[in] state: Firmware solution state
 */
static void update_mode_state(device_state_t *dev_state, struct soln_state *state) {
  u32 elapsed = elapsed_ms(&state->dgnss.systime);

  if (elapsed >= LED_MODE_TIMEOUT_MS) {
    dev_state->mode_state = MODE_NO_RTK;
  } else {
    switch (state->dgnss.mode) {
    case DGNSS_MODE_FIXED:
    {
      dev_state->mode_state = MODE_RTK_FIXED;
    } break;
    case DGNSS_MODE_FLOAT:
    {
      dev_state->mode_state = MODE_RTK_FLOAT;
    } break;
    case DGNSS_MODE_DGNSS:
    case DGNSS_MODE_INVALID:
    {
      dev_state->mode_state = MODE_NO_RTK;
    } break;
    case DGNSS_MODE_RESERVED:
    default:
    {
      //piksi_log(LOG_ERR,
      //          "Mode LED State Error: Unknown mode %d.", state->dgnss.mode);
      dev_state->mode_state = MODE_NO_RTK;
    } break;
    }
  }
}

/** Determine device state. Each LED has it's own specification how to behave
 *  under each state.
 * \param[out] dev_state: Device state reference
 */
static void get_device_state(device_state_t *dev_state) {
  struct soln_state state;
  firmware_state_get(&state);

  update_pos_state(dev_state, &state);

  update_link_state(dev_state);

  u8 base_obs_msg_counter = firmware_state_obs_counter_get();
  update_obs_state(dev_state, base_obs_msg_counter);

  update_mode_state(dev_state, &state);
}

/** Handle POS LED state.
 *
 * \param[in,out] s       Current LED state.
 * \param[in] dev_state   Current device state.
 *
 */
static void handle_pos(rgb_led_state_t *s, device_state_t *dev_state) {
  static blinker_state_t blinker_state;
  rgb_led_state_t active_pos_color = LED_COLOR_ORANGE;

  switch (dev_state->pos_state) {
  case POS_NO_SIGNAL:
  {
    blinker_state.mode = LED_OFF;
  } break;
  case POS_ANTENNA:
  {
    blinker_state.mode = LED_BLINK_SLOW;
  } break;
  case POS_INS_DEAD_RECK:
    active_pos_color = LED_COLOR_PURPLE;
  case POS_TRK_AT_LEAST_FOUR:
  {
    blinker_state.mode = LED_BLINK_FAST;
  } break;
  case POS_INS_GNSS:
    active_pos_color = LED_COLOR_PURPLE;
  case POS_GNSS:
  {
    blinker_state.mode = LED_ON;
  } break;
  default:
  {
    assert(!"Unknown mode");
  } break;
  }

  *s = blinker_update(&blinker_state) ? active_pos_color : LED_COLOR_OFF;
}

/** Handle LINK LED state. LED state changes according to received remote OBS
 *  rate.
 *
 * \param[in,out] s   Current LED state.
 *
 */
static void handle_link(rgb_led_state_t *s, device_state_t *dev_state) {
  static bool on_off = false;

  if (dev_state->obs_state == OBS_EVENT) {
    on_off = !on_off;
  } else {
    on_off = false;
  }

  if (dev_state->link_state == LINK_NETWORK_AVAILABLE) {
    *s = on_off ? LED_COLOR_OFF : LED_COLOR_RED;
  } else {
    *s = on_off ? LED_COLOR_RED : LED_COLOR_OFF;
  }
}

/** Handle MODE LED state.
 *
 * \param[in,out] s       Current LED state.
 * \param[in] dev_state   Current device state.
 *
 */
static void handle_mode(rgb_led_state_t *s, device_state_t *dev_state) {
  static blinker_state_t blinker_state;

  switch (dev_state->mode_state) {
    case MODE_NO_RTK:
      blinker_state.mode = LED_OFF;
      break;
    case MODE_RTK_FLOAT:
      blinker_state.mode = LED_BLINK_SLOW;
      break;
    case MODE_RTK_FIXED:
      blinker_state.mode = LED_ON;
      break;
    default:
      assert(!"Unknown mode");
      break;
  }

  *s = blinker_update(&blinker_state) ? LED_COLOR_BLUE : LED_COLOR_OFF;
}

static void * manage_led_thread(void *arg) {
  bool is_duro = (bool)arg;

  while (!firmware_state_heartbeat_seen())
    usleep(MANAGE_LED_THREAD_PERIOD_MS * 1000);

  led_adp8866_init(is_duro);
  {
    led_adp8866_led_state_t init_states[LED_ADP8866_LED_COUNT];
    for (int i = 0; i < LED_ADP8866_LED_COUNT; i++) {
      init_states[i].led = i;
      init_states[i].brightness = 255;
    }
    led_adp8866_leds_set(init_states,
                         sizeof(init_states) / sizeof(init_states[0]));
    sleep(2);
  }

  while (true) {
    device_state_t dev_state;
    get_device_state(&dev_state);
    rgb_led_state_t pos_state;
    handle_pos(&pos_state, &dev_state);

    rgb_led_state_t link_state;
    handle_link(&link_state, &dev_state);

    rgb_led_state_t mode_state;
    handle_mode(&mode_state, &dev_state);

    led_adp8866_led_state_t led_states[] = {
        {.led = LED_POS_R, .brightness = pos_state.r},
        {.led = LED_POS_G, .brightness = pos_state.g},
        {.led = LED_POS_B, .brightness = pos_state.b},
        {.led = LED_LINK_R, .brightness = link_state.r},
        {.led = LED_LINK_G, .brightness = link_state.g},
        {.led = LED_LINK_B, .brightness = link_state.b},
        {.led = LED_MODE_R, .brightness = mode_state.r},
        {.led = LED_MODE_G, .brightness = mode_state.g},
        {.led = LED_MODE_B, .brightness = mode_state.b}};
    led_adp8866_leds_set(led_states,
                         sizeof(led_states) / sizeof(led_states[0]));
    usleep(MANAGE_LED_THREAD_PERIOD_MS * 1000);
  }
  return NULL;
}

void sigbus_handler(int signum) {
  /* Just don't crash if the file is empty */
  lseek(network_available_fd, 0, SEEK_SET);
  write(network_available_fd, "0", 1);
}

void manage_led_setup(bool is_duro) {
  const char* network_available_path = "/var/run/network_available";
  network_available_fd = open(network_available_path, O_CREAT | O_RDONLY, 0644);
  if (network_available_fd < 0) {
    piksi_log(LOG_ERR, "failed to open file: %s", network_available_path);
    exit(1);
  }
  network_available = mmap(NULL, 4, PROT_READ, MAP_SHARED, network_available_fd, 0);
  signal(SIGBUS, sigbus_handler);
  pthread_t thread;
  pthread_create(&thread, NULL, manage_led_thread, (void*)is_duro);
}
