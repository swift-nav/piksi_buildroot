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
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libpiksi/logging.h>
#include <libpiksi/settings_client.h>
#include <libpiksi/util.h>

#include <libsbp/sbp.h>
#include <libsbp/vehicle.h>

#include <libsettings/settings.h>

#include "an_packet_protocol.h"
#include "sbp.h"
#include "obdii_odometer_packets.h"

#define PROGRAM_NAME "sbp_anpp_bridge"

#define ANPP_SUB_ENDPOINT "ipc:///var/run/sockets/anpp_internal.pub" /* ANPP Internal Out */
#define ANPP_SUB_METRICS "anpp/sub"

#define ANPP_PUB_ENDPOINT "ipc:///var/run/sockets/anpp_internal.sub" /* ANPP Internal In */
#define ANPP_PUB_METRICS "anpp/pub"

#define M_TO_MM(M) ((M)*1000)

#define SBP_MSG_ODOMETRY_FLAGS_NOTIME (0x00u)
#define SBP_MSG_ODOMETRY_FLAGS_GPSTIME (0x01u)
#define SBP_MSG_ODOMETRY_FLAGS_SYSTIME (0x02u)

#define SBP_MSG_ODOMETRY_FLAGS_SRC0 (0x00u)
#define SBP_MSG_ODOMETRY_FLAGS_SRC1 (0x04u)
#define SBP_MSG_ODOMETRY_FLAGS_SRC2 (0x08u)
#define SBP_MSG_ODOMETRY_FLAGS_SRC3 (0x0Cu)

bool anpp_debug = false;

pk_endpoint_t *anpp_pub = NULL;

static void convert_anpp_odometer_to_sbp(odometer_packet_t *an_odo_packet, msg_odometry_t *odo_msg)
{
  time_t now = time(NULL);
  odo_msg->tow = (u32)get_time_ms();
  odo_msg->velocity = (s32)M_TO_MM(an_odo_packet->speed);
  odo_msg->flags = (u8)(SBP_MSG_ODOMETRY_FLAGS_SRC0 | SBP_MSG_ODOMETRY_FLAGS_SYSTIME);
}

static void send_sbp_odometry_msg(odometer_packet_t *an_odo_packet)
{
  msg_odometry_t odo_msg;
  convert_anpp_odometer_to_sbp(an_odo_packet, &odo_msg);
  sbp_send_message(SBP_MSG_ODOMETRY, sizeeof(odo_msg), &odo_msg, sbp_sender_id_get(), NULL);
}

static int anpp2sbp_decode_frame(const u8 *data, const size_t length, void *context)
{
  (void)context;
  an_decoder_t an_decoder;
  an_packet_t *an_packet;

  odometer_packet_t odometer_packet;

  if (length == 0) {
    return 0; // process called with no data
  }

  an_decoder_initialise(&an_decoder);
  if (length > AN_DECODE_BUFFER_SIZE) {
    PK_LOG_ANNO(LOG_ERR,
                "ANPP Frame larger than max decode size (%ld bytes). Dropping data.",
                (length - AN_DECODE_BUFFER_SIZE));
    length = AN_DECODE_BUFFER_SIZE;
  }

  memcpy(an_decoder_pointer(&an_decoder), data, length);
  an_decoder_increment(&an_decoder, length);

  /* decode all the packets in the buffer */
  while ((an_packet = an_packet_decode(&an_decoder)) != NULL) {
    if (an_packet->id == packet_id_odometer) /* odometer packet */
    {
      /* copy all the binary data into the typedef struct for the packet */
      /* this allows easy access to all the different values             */
      if (decode_odometer_packet(&odometer_packet, an_packet) == 0) {
        piksi_log(
          LOG_DEBUG,
          "Odometer Packet: Delay = %f s, Speed = %f m/s, Distance = %f m, Reverse Detection = %d\n",
          odometer_packet.delay,
          odometer_packet.speed,
          odometer_packet.distance_travelled,
          odometer_packet.flags.b.reverse_detection_supported);
        send_sbp_odometry_msg(odometer_packet);
      }
    } else {
      printf("Packet ID %u of Length %u\n", an_packet->id, an_packet->length);
    }

    /* Ensure that you free the an_packet when your done with it or you will leak memory */
    an_packet_free(&an_packet);
  }

  return 0;
}

static void anpp_reader_handler(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)handle;
  (void)status;

  pk_endpoint_t *anpp_sub_ept = (pk_endpoint_t *)context;
  if (pk_endpoint_receive(anpp_sub_ept, anpp2sbp_decode_frame, NULL) != 0) {
    piksi_log(LOG_ERR,
              "%s: error in %s (%s:%d): %s",
              __FUNCTION__,
              "pk_endpoint_receive",
              __FILE__,
              __LINE__,
              pk_endpoint_strerror());
  }
}

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMisc options");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_DEBUG = 1,
  };

  const struct option long_opts[] = {
    {"debug", no_argument, 0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_DEBUG: {
      anpp_debug = true;
    } break;

    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }
  return 0;
}

static int cleanup(pk_endpoint_t **anpp_ept_loc, int status);

int main(int argc, char *argv[])
{
  pk_loop_t *loop = NULL;
  pk_endpoint_t *anpp_sub = NULL;

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(cleanup(&anpp_sub, EXIT_FAILURE));
  }

  if (sbp_init() != 0) {
    piksi_log(LOG_ERR, "error initializing SBP");
    exit(cleanup(&anpp_sub, EXIT_FAILURE));
  }

  anpp_pub = pk_endpoint_create(pk_endpoint_config()
                                  .endpoint(ANPP_PUB_ENDPOINT)
                                  .identity(ANPP_PUB_METRICS)
                                  .type(PK_ENDPOINT_PUB)
                                  .get());
  if (anpp_pub == NULL) {
    piksi_log(LOG_ERR, "error creating PUB socket");
    exit(cleanup(&anpp_sub, EXIT_FAILURE));
  }

  anpp_sub = pk_endpoint_create(pk_endpoint_config()
                                  .endpoint(ANPP_SUB_ENDPOINT)
                                  .identity(ANPP_SUB_METRICS)
                                  .type(PK_ENDPOINT_SUB)
                                  .get());
  if (anpp_sub == NULL) {
    piksi_log(LOG_ERR, "error creating SUB socket");
    exit(cleanup(&anpp_sub, EXIT_FAILURE));
  }

  loop = sbp_get_loop();
  if (loop == NULL) {
    exit(cleanup(&anpp_sub, EXIT_FAILURE));
  }

  if (pk_loop_endpoint_reader_add(loop, anpp_sub, anpp_reader_handler, anpp_sub) == NULL) {
    piksi_log(LOG_ERR, "error adding reader");
    exit(cleanup(&anpp_sub, EXIT_FAILURE));
  }

  sbp_run();

  exit(cleanup(&anpp_sub, EXIT_SUCCESS));
}

static int cleanup(pk_endpoint_t **anpp_ept_loc, int status)
{
  pk_endpoint_destroy(anpp_ept_loc);
  sbp_deinit();
  logging_deinit();
  return status;
}
