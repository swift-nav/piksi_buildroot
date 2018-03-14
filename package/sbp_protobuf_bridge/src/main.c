#include <assert.h>
#include <czmq.h>
#include <pb_common.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include <getopt.h>
#include <libpiksi/sbp_zmq_pubsub.h>
#include <libpiksi/sbp_zmq_rx.h>
#include <libpiksi/settings.h>
#include <libpiksi/util.h>
#include <libpiksi/logging.h>
#include <libsbp/navigation.h>
#include <libsbp/sbp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "navigation.pb.h"

#define PROGRAM_NAME "sbp_protobuf_bridge"

#define PROTOBUF_SUB_ENDPOINT  ">tcp://127.0.0.1:46010"  /* PROTOBUF Internal Out */
#define PROTOBUF_PUB_ENDPOINT  ">tcp://127.0.0.1:46011"  /* PROTOBUF Internal In */
#define SBP_SUB_ENDPOINT    ">tcp://127.0.0.1:43030"  /* SBP External Out */
#define SBP_PUB_ENDPOINT    ">tcp://127.0.0.1:43031"  /* SBP External In */

static void p2s_llh(pb_istream_t *stream)
{

  piksi_log(LOG_DEBUG, "prot llh pos decoding");
  swiftnav_sbp_navigation_MsgPosLlh pb_pos = swiftnav_sbp_navigation_MsgPosLlh_init_zero;
  bool status = pb_decode(stream, swiftnav_sbp_navigation_MsgPosLlh_fields, &pb_pos);
  if (!status) {
    piksi_log(LOG_ERR, "Decoding failed: %s\n", PB_GET_ERROR(stream));
  }
  piksi_log(LOG_DEBUG, "pb pos tow %d", pb_pos.tow);


  msg_pos_llh_t sbp_pos = {
    .tow = pb_pos.tow,
    .lat = pb_pos.lat,
    .lon = pb_pos.lon,
    .height = pb_pos.height,
    .h_accuracy = pb_pos.h_accuracy,
    .v_accuracy = pb_pos.v_accuracy,
    .n_sats = pb_pos.n_sats,
    .flags = pb_pos.flags
  };

  piksi_log(LOG_DEBUG, "sbb pos tow %d", sbp_pos.tow);

}


static void protobuf2sbp_decode_frame(const uint8_t *frame, uint32_t frame_length)
{
  uint16_t msg_id = (frame[1] << 8) | frame[0];
  // piksi_log(LOG_DEBUG, "Proto Frame Callback!! %d  %d: %x %x %x %x %x %x %x %x", frame_length, msg_id, frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6], frame[7]);
  pb_istream_t stream = pb_istream_from_buffer(frame, frame_length-2);
  switch (msg_id) {
    case SBP_MSG_POS_LLH:
      p2s_llh(&stream);
      break;
    default:
      piksi_log(LOG_ERR, "Unhandled proto msg %d", msg_id);
      return;
  }

}

static int protobuf_reader_handler(zloop_t *zloop, zsock_t *zsock, void *arg)
{
  (void)zloop;
  (void)arg;
  zmsg_t *msg;
  while (1) {
    msg = zmsg_recv(zsock);
    if (msg != NULL) {
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in zmsg_recv()");
      return -1;
    }
  }

  zframe_t *frame;
  for (frame = zmsg_first(msg); frame != NULL; frame = zmsg_next(msg)) {
    //rtcm2sbp_decode_frame(zframe_data(frame), zframe_size(frame), &state);
    // piksi_log(LOG_DEBUG, "Proto Frame Callback!! Size: %d", zframe_size(frame));
    protobuf2sbp_decode_frame(zframe_data(frame), zframe_size(frame));
  }

  zmsg_destroy(&msg);
  return 0;
}

static int protobuf_send_frame(u8 *data, u16 length, u16 msg_id, u16 sender_id, zsock_t *zsock)
{
  static const u8 preamble[2] = { 0xFC, 0xCC };
  zmsg_t *msg = zmsg_new();
  if (msg == NULL) {
    piksi_log(LOG_ERR, "error in zmsg_new()");
    return -1;
  }

  length += 2; // for the msg id in header
  length += 2; // for the sender id in header
  //HAAAAACKKKY FRAMING
  if (zmsg_addmem(msg, preamble, sizeof(preamble)) != 0) {
    piksi_log(LOG_ERR, "error in zmsg_addmem()");
    zmsg_destroy(&msg);
    return -1;
  }
  if (zmsg_addmem(msg, (u8 *)&length, sizeof(length)) != 0) {
    piksi_log(LOG_ERR, "error in zmsg_addmem()");
    zmsg_destroy(&msg);
    return -1;
  }
  if (zmsg_addmem(msg, (u8 *)&msg_id, sizeof(msg_id)) != 0) {
    piksi_log(LOG_ERR, "error in zmsg_addmem()");
    zmsg_destroy(&msg);
    return -1;
  }
  if (zmsg_addmem(msg, (u8 *)&sender_id, sizeof(sender_id)) != 0) {
    piksi_log(LOG_ERR, "error in zmsg_addmem()");
    zmsg_destroy(&msg);
    return -1;
  }
  if (zmsg_addmem(msg, data, length) != 0) {
    piksi_log(LOG_ERR, "error in zmsg_addmem()");
    zmsg_destroy(&msg);
    return -1;
  }

  while (1) {
    int ret = zmsg_send(&msg, zsock);
    if (ret == 0) {
      /* Break on success */
      break;
    } else if (errno == EINTR) {
      /* Retry if interrupted */
      continue;
    } else {
      /* Return error */
      piksi_log(LOG_ERR, "error in zmsg_send()");
      zmsg_destroy(&msg);
      return ret;
    }
  }

  return 0;
}

static void pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)len;
  (void)msg;
  sbp_zmq_pubsub_ctx_t *pb_ctx = (sbp_zmq_pubsub_ctx_t *)context;

  msg_pos_llh_t *sbp_pos = (msg_pos_llh_t*)msg;

  swiftnav_sbp_navigation_MsgPosLlh pb_pos = {
    .tow = sbp_pos->tow,
    .lat = sbp_pos->lat,
    .lon = sbp_pos->lon,
    .height = sbp_pos->height,
    .h_accuracy = sbp_pos->h_accuracy,
    .v_accuracy = sbp_pos->v_accuracy,
    .n_sats = sbp_pos->n_sats,
    .flags = sbp_pos->flags
  };

  uint8_t buffer[128];
  size_t message_length;
  bool status;


  /* Create a stream that will write to our buffer. */
  pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
  status = pb_encode(&stream, swiftnav_sbp_navigation_MsgPosLlh_fields, &pb_pos);

  if (!status)
  {
      piksi_log(LOG_ERR, "Decoding failed: %s\n", PB_GET_ERROR(&stream));
  }

  message_length = stream.bytes_written;

  // piksi_log(LOG_DEBUG, "LLH Callback!!");
  if (protobuf_send_frame(buffer, message_length, SBP_MSG_POS_LLH, sender_id, sbp_zmq_pubsub_zsock_pub_get(pb_ctx)) != 0) {
    piksi_log(LOG_ERR, "Failed to send protobuf LLH");
  }
}

static int cleanup(sbp_zmq_pubsub_ctx_t **sbp_ctx_loc,
                   sbp_zmq_pubsub_ctx_t **pb_ctx_loc,
                   int status);

int main(int argc, char *argv[])
{
  (void)argc;
  (void)argv;
  sbp_zmq_pubsub_ctx_t *sbp_ctx = NULL;
  sbp_zmq_pubsub_ctx_t *pb_ctx = NULL;

  logging_init(PROGRAM_NAME);

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  sbp_ctx = sbp_zmq_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (sbp_ctx == NULL) {
    piksi_log(LOG_ERR, "sbp_ctx error");
    return cleanup(&sbp_ctx, &pb_ctx, EXIT_FAILURE);
  }

  pb_ctx = sbp_zmq_pubsub_create(PROTOBUF_PUB_ENDPOINT, PROTOBUF_SUB_ENDPOINT);
  if (pb_ctx == NULL) {
    piksi_log(LOG_ERR, "pb_ctx error");
    return cleanup(&sbp_ctx, &pb_ctx, EXIT_FAILURE);
  }

  if (sbp_zmq_rx_callback_register(sbp_zmq_pubsub_rx_ctx_get(sbp_ctx),
                                   SBP_MSG_POS_LLH,
                                   pos_llh_callback,
                                   pb_ctx, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting POS_LLH callback");
    return cleanup(&sbp_ctx, &pb_ctx, EXIT_FAILURE);
  }

  if (zloop_reader(sbp_zmq_pubsub_zloop_get(sbp_ctx),
                   sbp_zmq_pubsub_zsock_sub_get(pb_ctx),
                   protobuf_reader_handler,
                   sbp_ctx) != 0) {
    piksi_log(LOG_ERR, "error adding pb_ctx reader to sbp_ctx zloop");
    return cleanup(&sbp_ctx, &pb_ctx, EXIT_FAILURE);
  }

  zmq_simple_loop(sbp_zmq_pubsub_zloop_get(sbp_ctx));

  return cleanup(&sbp_ctx, &pb_ctx, EXIT_SUCCESS);
}

static int cleanup(sbp_zmq_pubsub_ctx_t **sbp_ctx_loc,
                   sbp_zmq_pubsub_ctx_t **pb_ctx_loc,
                   int status) {
  sbp_zmq_pubsub_destroy(sbp_ctx_loc);
  sbp_zmq_pubsub_destroy(pb_ctx_loc);
  logging_deinit();
  return status;
}
