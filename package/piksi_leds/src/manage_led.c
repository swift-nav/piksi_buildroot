/*
 * Copyright (C) 2016 Swift Navigation Inc.
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
#include <hal.h>

#include "base_obs.h"
#include "me_calc_pvt.h"
#include "peripherals/antenna.h"
#include "peripherals/led_adp8866.h"
#include "piksi_systime.h"
#include "starling_calc_pvt.h"

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

#define MANAGE_LED_THREAD_PERIOD_MS 10
#define SLOW_BLINK_PERIOD_MS 500
#define FAST_BLINK_PERIOD_MS 250

#define LED_MODE_TIMEOUT_MS 1500

#define MANAGE_LED_THREAD_STACK 2000
#define MANAGE_LED_THREAD_PRIO (NORMALPRIO + 10)

typedef struct {
  u8 r;
  u8 g;
  u8 b;
} rgb_led_state_t;

typedef enum { LED_OFF, LED_BLINK_SLOW, LED_BLINK_FAST, LED_ON } blink_mode_t;

typedef enum {
  DEV_NO_SIGNAL,
  DEV_ANTENNA,
  DEV_TRK_AT_LEAST_FOUR,
  DEV_SPS,
  DEV_FLOAT,
  DEV_FIXED
} device_state_t;

typedef struct {
  blink_mode_t mode;            /* Current mode */
  piksi_systime_t state_change; /* Time of previous on/off event */
  bool on_off;                  /* Current state */
} blinker_state_t;

static THD_WORKING_AREA(wa_manage_led_thread, MANAGE_LED_THREAD_STACK);

/** Update LED blink state.
 *
 * \param[in] b            LED state information.
 *
 * \return bool, TRUE for ON, FALSE for OFF.
 */
static bool blinker_update(blinker_state_t *b) {
  u16 period_ms;

  switch (b->mode) {
    case LED_OFF:
      return FALSE;
      break;
    case LED_ON:
      return TRUE;
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

  u32 elapsed = piksi_systime_elapsed_since_ms(&b->state_change);

  if (elapsed >= period_ms) {
    b->on_off = !b->on_off;
    piksi_systime_get(&b->state_change);
  }

  return b->on_off;
}

/** Determine device state. Each LED has it's own specification how to behave
 *  under each state.
 *
 * \return Device state.
 */
static device_state_t get_device_state(void) {
  /* Check for FIXED */
  soln_dgnss_stats_t stats = solution_last_dgnss_stats_get();
  s8 fix = piksi_systime_cmp(&PIKSI_SYSTIME_INIT, &stats.systime);

  if (fix) {
    u32 elapsed = piksi_systime_elapsed_since_ms(&stats.systime);
    if (elapsed < LED_MODE_TIMEOUT_MS) {
      return (FILTER_FIXED == stats.mode) ? DEV_FIXED : DEV_FLOAT;
    }
  }

  /* Check for SPS */
  piksi_systime_t t = solution_last_pvt_stats_get().systime;
  fix = piksi_systime_cmp(&PIKSI_SYSTIME_INIT, &t);

  if (fix) {
    u32 elapsed = piksi_systime_elapsed_since_ms(&t);

    /* PVT available */
    if (elapsed < LED_MODE_TIMEOUT_MS) {
      return DEV_SPS;
    }
  }

  /* Blink according to signals tracked */
  u8 signals_tracked = solution_last_stats_get().signals_tracked;

  if (signals_tracked >= 4) {
    return DEV_TRK_AT_LEAST_FOUR;
  }

  if (antenna_present()) {
    return DEV_ANTENNA;
  }

  return DEV_NO_SIGNAL;
}

/** Handle PV LED state.
 *
 * \param[in] dev_state   Current device state.
 *
 * \return bool, TRUE for ON, FALSE for OFF.
 */
static bool handle_pv(device_state_t dev_state) {
  switch (dev_state) {
    case DEV_NO_SIGNAL:
    case DEV_ANTENNA:
    case DEV_TRK_AT_LEAST_FOUR:
      return FALSE;
      break;
    case DEV_SPS:
    case DEV_FLOAT:
    case DEV_FIXED:
      return TRUE;
      break;
    default:
      assert(!"Unknown mode");
      break;
  }
}

/** Handle POS LED state.
 *
 * \param[in,out] s       Current LED state.
 * \param[in] dev_state   Current device state.
 *
 */
static void handle_pos(rgb_led_state_t *s, device_state_t dev_state) {
  static blinker_state_t blinker_state;

  switch (dev_state) {
    case DEV_NO_SIGNAL:
      blinker_state.mode = LED_OFF;
      break;
    case DEV_ANTENNA:
      blinker_state.mode = LED_BLINK_SLOW;
      break;
    case DEV_TRK_AT_LEAST_FOUR:
      blinker_state.mode = LED_BLINK_FAST;
      break;
    case DEV_SPS:
    case DEV_FLOAT:
    case DEV_FIXED:
      blinker_state.mode = LED_ON;
      break;
    default:
      assert(!"Unknown mode");
      break;
  }

  *s = blinker_update(&blinker_state) ? LED_COLOR_ORANGE : LED_COLOR_OFF;
}

/** Handle LINK LED state. LED state changes according to received remote OBS
 *  rate.
 *
 * \param[in,out] s   Current LED state.
 *
 */
static void handle_link(rgb_led_state_t *s) {
  static bool on_off = FALSE;
  static u8 last_base_obs_msg_counter = 0;

  u8 base_obs_msg_counter = base_obs_msg_counter_get();

  if (base_obs_msg_counter != last_base_obs_msg_counter) {
    last_base_obs_msg_counter = base_obs_msg_counter;
    on_off = !on_off;
  } else {
    on_off = FALSE;
  }

  *s = on_off ? LED_COLOR_RED : LED_COLOR_OFF;
}

/** Handle MODE LED state.
 *
 * \param[in,out] s       Current LED state.
 * \param[in] dev_state   Current device state.
 *
 */
static void handle_mode(rgb_led_state_t *s, device_state_t dev_state) {
  static blinker_state_t blinker_state;

  switch (dev_state) {
    case DEV_NO_SIGNAL:
    case DEV_ANTENNA:
    case DEV_TRK_AT_LEAST_FOUR:
    case DEV_SPS:
      blinker_state.mode = LED_OFF;
      break;
    case DEV_FLOAT:
      blinker_state.mode = LED_BLINK_SLOW;
      break;
    case DEV_FIXED:
      blinker_state.mode = LED_ON;
      break;
    default:
      assert(!"Unknown mode");
      break;
  }

  *s = blinker_update(&blinker_state) ? LED_COLOR_BLUE : LED_COLOR_OFF;
}

static void manage_led_thread(void *arg) {
  (void)arg;
  chRegSetThreadName("manage LED");

  /* Configure GPIO */
  chSysLock();
  { palSetLineMode(POS_VALID_GPIO_LINE, PAL_MODE_OUTPUT); }
  chSysUnlock();

  palClearLine(POS_VALID_GPIO_LINE);

  led_adp8866_init();

  while (TRUE) {
    device_state_t dev_state = get_device_state();
    rgb_led_state_t pos_state;
    handle_pos(&pos_state, dev_state);

    rgb_led_state_t link_state;
    handle_link(&link_state);

    rgb_led_state_t mode_state;
    handle_mode(&mode_state, dev_state);

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

    bool pv_state = handle_pv(dev_state);
    palWriteLine(POS_VALID_GPIO_LINE, pv_state ? PAL_HIGH : PAL_LOW);

    piksi_systime_sleep_ms(MANAGE_LED_THREAD_PERIOD_MS);
  }
}

void manage_led_setup(void) {
  chThdCreateStatic(wa_manage_led_thread,
                    sizeof(wa_manage_led_thread),
                    MANAGE_LED_THREAD_PRIO,
                    manage_led_thread,
                    NULL);
}
