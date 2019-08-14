/****************************************************************/
/*                                                              */
/*          Advanced Navigation Packet Protocol Library         */
/*      C Language Dynamic OBDII Odometer SDK, Version 1.0      */
/*   Copyright 2014, Xavier Orr, Advanced Navigation Pty Ltd    */
/*                                                              */
/****************************************************************/
/*
 * Copyright (C) 2014 Advanced Navigation Pty Ltd
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define MAXIMUM_PACKET_PERIODS 50
#define MAXIMUM_DETAILED_SATELLITES 32

#define START_SYSTEM_PACKETS 0
#define START_STATE_PACKETS 20
#define START_CONFIGURATION_PACKETS 180

typedef enum {
  packet_id_acknowledge,
  packet_id_request,
  packet_id_boot_mode,
  packet_id_device_information,
  packet_id_restore_factory_settings,
  packet_id_reset,
  end_system_packets,

  packet_id_odometer = 67,
  end_state_packets,

  end_configuration_packets = START_CONFIGURATION_PACKETS
} packet_id_e;

/* start of system packets typedef structs */

typedef enum {
  acknowledge_success,
  acknowledge_failure_crc,
  acknowledge_failure_length,
  acknowledge_failure_range,
  acknowledge_failure_flash,
  acknowledge_failure_not_ready,
  acknowledge_failure_unknown_packet
} acknowledge_result_e;

typedef struct {
  uint8_t packet_id;
  uint16_t packet_crc;
  uint8_t acknowledge_result;
} acknowledge_packet_t;

typedef enum { boot_mode_bootloader, boot_mode_main_program } boot_mode_e;

typedef struct {
  uint8_t boot_mode;
} boot_mode_packet_t;

typedef struct {
  uint32_t software_version;
  uint32_t device_id;
  uint32_t hardware_revision;
  uint32_t serial_number[3];
} device_information_packet_t;

/* start of state packets typedef structs */

typedef struct {
  float delay;
  float speed;
  float distance_travelled;
  union {
    uint8_t r;
    struct {
      int reverse_detection_supported : 1;
    } b;
  } flags;
} odometer_packet_t;

int decode_acknowledge_packet(acknowledge_packet_t *acknowledge_packet, an_packet_t *an_packet);
an_packet_t *encode_request_packet(uint8_t requested_packet_id);
int decode_boot_mode_packet(boot_mode_packet_t *boot_mode_packet, an_packet_t *an_packet);
an_packet_t *encode_boot_mode_packet(boot_mode_packet_t *boot_mode_packet);
int decode_device_information_packet(device_information_packet_t *device_information_packet,
                                     an_packet_t *an_packet);
an_packet_t *encode_restore_factory_settings_packet();
int decode_odometer_packet(odometer_packet_t *odometer_packet, an_packet_t *an_packet);

#ifdef __cplusplus
}
#endif
