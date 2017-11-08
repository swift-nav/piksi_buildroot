/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "led_adp8866.h"
#include "led_adp8866_regs.h"

#include <ch.h>
#include <hal.h>
#include <libswiftnav/logging.h>

#include <string.h>

#define LED_I2C_ADDR 0x27
#define LED_I2C_TIMEOUT MS2ST(100)

#define LED_LEVEL_SET_VALUE 0x20

static struct I2CDriver *led_i2c = &I2CD2;

static const I2CConfig led_i2c_config = LED_I2C_CONFIG;

static u8 brightness_cache[LED_ADP8866_LED_COUNT] = {0};

/** Lock and start the I2C driver.
 */
static void i2c_open(void) {
  i2cAcquireBus(led_i2c);
  i2cStart(led_i2c, &led_i2c_config);
}

/** Unlock and stop the I2C driver.
 */
static void i2c_close(void) {
  i2cStop(led_i2c);
  i2cReleaseBus(led_i2c);
}

/*
 * Duro connected different outputs of the adp8866 driver to the RGB
 * Unfortunately we need different routing on RGB for addressing this
 * case
 */

static u8 isc_route[9] = {0};

static void init_reg_isc(bool is_duro) {
  int i;
  if (is_duro) {
    isc_route[0] = 0x23U + 8;
    for (i = 2; i <= 9; i++) {
      isc_route[i - 1] = 0x23U + i - 2;
    }
  } else {
    for (i = 1; i <= 9; i++) {
      isc_route[i - 1] = 0x23U + i - 1;
    }
  }
}

/** Perform an I2C read operation.
 *
 * \param addr          Register address.
 * \param data          Output data.
 *
 * \return MSG_OK if the operation succeeded, error message otherwise.
 */
static msg_t i2c_read(u8 addr, u8 *data) {
  return i2cMasterTransmitTimeout(
      led_i2c, LED_I2C_ADDR, &addr, 1, data, 1, LED_I2C_TIMEOUT);
}

/** Perform an I2C write operation.
 *
 * \param addr          Register address.
 * \param data          Data to write.
 *
 * \return MSG_OK if the operation succeeded, error message otherwise.
 */
static msg_t i2c_write(u8 addr, u8 data) {
  u8 buf[2] = {addr, data};
  return i2cMasterTransmitTimeout(
      led_i2c, LED_I2C_ADDR, buf, sizeof(buf), NULL, 0, LED_I2C_TIMEOUT);
}

/** Verify the contents of the MFDVID register.
 *
 * \return true if the operation succeeded, false otherwise.
 */
static bool id_check(void) {
  bool read_ok;
  u8 mfdvid;
  i2c_open();
  { read_ok = (i2c_read(LED_ADP8866_REG_MFDVID, &mfdvid) == MSG_OK); }
  i2c_close();

  if (!read_ok) {
    log_warn("Could not read LED driver ID register");
    return false;
  } else if (mfdvid !=
             ((LED_ADP8866_MFDVID_MFID << LED_ADP8866_MFDVID_MFID_Pos) |
              (LED_ADP8866_MFDVID_DVID << LED_ADP8866_MFDVID_DVID_Pos))) {
    log_warn("Read invalid LED driver ID: %02x", mfdvid);
    return false;
  }

  return true;
}

/** Configure the LED driver.
 *
 * \return true if the operation succeeded, false otherwise.
 */
static bool configure(void) {
  bool ret = true;

  i2c_open();
  {
    /* Configure all LEDs as independent sinks */
    if (i2c_write(LED_ADP8866_REG_CFGR,
                  (LED_ADP8866_BLSEL_IS << LED_ADP8866_CFGR_D9SEL_Pos)) !=
        MSG_OK) {
      ret = false;
    }

    if (i2c_write(LED_ADP8866_REG_BLSEL,
                  ((LED_ADP8866_BLSEL_IS << LED_ADP8866_BLSEL_D1SEL_Pos) |
                   (LED_ADP8866_BLSEL_IS << LED_ADP8866_BLSEL_D2SEL_Pos) |
                   (LED_ADP8866_BLSEL_IS << LED_ADP8866_BLSEL_D3SEL_Pos) |
                   (LED_ADP8866_BLSEL_IS << LED_ADP8866_BLSEL_D4SEL_Pos) |
                   (LED_ADP8866_BLSEL_IS << LED_ADP8866_BLSEL_D5SEL_Pos) |
                   (LED_ADP8866_BLSEL_IS << LED_ADP8866_BLSEL_D6SEL_Pos) |
                   (LED_ADP8866_BLSEL_IS << LED_ADP8866_BLSEL_D7SEL_Pos) |
                   (LED_ADP8866_BLSEL_IS << LED_ADP8866_BLSEL_D8SEL_Pos))) !=
        MSG_OK) {
      ret = false;
    }

    /* Enable current scaling */
    if (i2c_write(LED_ADP8866_REG_LVL_SEL1,
                  ((LED_LEVEL_SET_VALUE << LED_ADP8866_LVL_SEL1_LEVEL_SET_Pos) |
                   (LED_ADP8866_LVL_SEL_SCALED
                    << LED_ADP8866_LVL_SEL1_D9LVL_Pos))) != MSG_OK) {
      ret = false;
    }

    if (i2c_write(
            LED_ADP8866_REG_LVL_SEL2,
            ((LED_ADP8866_LVL_SEL_SCALED << LED_ADP8866_LVL_SEL2_D1LVL_Pos) |
             (LED_ADP8866_LVL_SEL_SCALED << LED_ADP8866_LVL_SEL2_D2LVL_Pos) |
             (LED_ADP8866_LVL_SEL_SCALED << LED_ADP8866_LVL_SEL2_D3LVL_Pos) |
             (LED_ADP8866_LVL_SEL_SCALED << LED_ADP8866_LVL_SEL2_D4LVL_Pos) |
             (LED_ADP8866_LVL_SEL_SCALED << LED_ADP8866_LVL_SEL2_D5LVL_Pos) |
             (LED_ADP8866_LVL_SEL_SCALED << LED_ADP8866_LVL_SEL2_D6LVL_Pos) |
             (LED_ADP8866_LVL_SEL_SCALED << LED_ADP8866_LVL_SEL2_D7LVL_Pos) |
             (LED_ADP8866_LVL_SEL_SCALED << LED_ADP8866_LVL_SEL2_D8LVL_Pos))) !=
        MSG_OK) {
      ret = false;
    }

    /* Set current to zero for all independent sinks */
    for (u32 i = 1; i <= LED_ADP8866_LED_COUNT; i++) {
      if (i2c_write(isc_route[i - 1], 0) != MSG_OK) {
        ret = false;
      }
    }

    /* Enable all independent sinks */
    if (i2c_write(LED_ADP8866_REG_ISCC1, (1 << LED_ADP8866_ISCC1_SC9_EN_Pos)) !=
        MSG_OK) {
      ret = false;
    }

    if (i2c_write(LED_ADP8866_REG_ISCC2,
                  ((1 << LED_ADP8866_ISCC2_SC1_EN_Pos) |
                   (1 << LED_ADP8866_ISCC2_SC2_EN_Pos) |
                   (1 << LED_ADP8866_ISCC2_SC3_EN_Pos) |
                   (1 << LED_ADP8866_ISCC2_SC4_EN_Pos) |
                   (1 << LED_ADP8866_ISCC2_SC5_EN_Pos) |
                   (1 << LED_ADP8866_ISCC2_SC6_EN_Pos) |
                   (1 << LED_ADP8866_ISCC2_SC7_EN_Pos) |
                   (1 << LED_ADP8866_ISCC2_SC8_EN_Pos))) != MSG_OK) {
      ret = false;
    }

    /* Normal mode */
    if (i2c_write(LED_ADP8866_REG_MDCR, (1 << LED_ADP8866_MDCR_NSTBY_Pos)) !=
        MSG_OK) {
      ret = false;
    }
  }
  i2c_close();

  return ret;
}

/** Set LED states.
 *
 * \param led_states        Array of LED states to set.
 * \param led_states_count  Number of elements in led_states.
 *
 * \return true if the operation succeeded, false otherwise.
 */
static bool leds_set(const led_adp8866_led_state_t *led_states,
                     u32 led_states_count) {
  bool ret = true;

  i2c_open();
  {
    for (u32 i = 0; i < led_states_count; i++) {
      const led_adp8866_led_state_t *led_state = &led_states[i];

      /* Write ISCn */
      if (i2c_write(isc_route[led_state->led],
                    (led_state->brightness << LED_ADP8866_ISCn_SCDn_Pos)) !=
          MSG_OK) {
        ret = false;
      } else {
        /* Update cache */
        brightness_cache[led_state->led] = led_state->brightness;
      }
    }
  }
  i2c_close();

  return ret;
}

/** Get an array of modified LED states.
 *
 * \param led_states        Input array of LED states.
 * \param led_states_count  Number of elements in led_states.
 * \param output_states     Output array of modified LED states.
 *
 * \return Number of elements written to output_states.
 */
static u32 modified_states_get(const led_adp8866_led_state_t *input_states,
                               u32 input_states_count,
                               led_adp8866_led_state_t *output_states) {
  u32 output_states_count = 0;

  for (u32 i = 0; i < input_states_count; i++) {
    const led_adp8866_led_state_t *input_state = &input_states[i];
    led_adp8866_led_state_t *output_state = &output_states[output_states_count];

    /* Compare brightness */
    if (input_state->brightness == brightness_cache[input_state->led]) {
      continue;
    }

    /* Append to output states */
    *output_state = *input_state;
    output_states_count++;
  }

  return output_states_count;
}

/** Initialize the LED driver.
 */
void led_adp8866_init(void) {
  led_i2c = board_is_duro() ? &I2CD1 : &I2CD2;

  init_reg_isc(board_is_duro());

  if (!id_check()) {
    return;
  }

  if (!configure()) {
    log_warn("Failed to configure LED driver");
  }

  led_adp8866_led_state_t led_states[LED_ADP8866_LED_COUNT];
  for (u32 i = 0; i < LED_ADP8866_LED_COUNT; i++) {
    led_adp8866_led_state_t *led_state = &led_states[i];
    led_state->led = i;
    led_state->brightness = 0;
  }

  if (!leds_set(led_states, LED_ADP8866_LED_COUNT)) {
    log_warn("Failed to initialize LED states");
  }
}

/** Set an LED state.
 *
 * \param led_state         LED state to set.
 *
 * \return true if the operation succeeded, false otherwise.
 */
bool led_adp8866_led_set(const led_adp8866_led_state_t *led_state) {
  return led_adp8866_leds_set(led_state, 1);
}

/** Set LED states.
 *
 * \param led_states        Array of LED states to set.
 * \param led_states_count  Number of elements in led_states.
 *
 * \return true if the operation succeeded, false otherwise.
 */
bool led_adp8866_leds_set(const led_adp8866_led_state_t *led_states,
                          u32 led_states_count) {
  led_adp8866_led_state_t modified_states[LED_ADP8866_LED_COUNT];
  u32 modified_states_count =
      modified_states_get(led_states, led_states_count, modified_states);

  if (modified_states_count == 0) {
    return true;
  }

  return leds_set(modified_states, modified_states_count);
}
