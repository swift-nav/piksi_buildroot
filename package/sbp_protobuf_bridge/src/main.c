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
#include "sbp.h"

#define PROGRAM_NAME "sbp_protobuf_bridge"

#define PROTOBUF_SUB_ENDPOINT  ">tcp://127.0.0.1:46010"  /* PROTOBUF Internal Out */
#define PROTOBUF_PUB_ENDPOINT  ">tcp://127.0.0.1:46011"  /* PROTOBUF Internal In */
#define SBP_SUB_ENDPOINT    ">tcp://127.0.0.1:43030"  /* SBP External Out */
#define SBP_PUB_ENDPOINT    ">tcp://127.0.0.1:43031"  /* SBP External In */


static void pos_llh_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void)sender_id;
  (void)len;
  (void)msg;
  (void)context;
  //msg_pos_llh_t *pos = (msg_pos_llh_t*)msg;

  piksi_log(LOG_DEBUG, "LLH Callback!!");
  //sbp_send_message(&udp_context.sbp_state, SBP_MSG_POS_LLH, sender_id, len, msg, udp_write_callback);
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

  if (sbp_callback_register(SBP_MSG_POS_LLH, pos_llh_callback, pb_ctx) != 0) {
    piksi_log(LOG_ERR, "error setting POS_LLH callback");
    return cleanup(&sbp_ctx, &pb_ctx, EXIT_FAILURE);
  }
}

static int cleanup(sbp_zmq_pubsub_ctx_t **sbp_ctx_loc,
                   sbp_zmq_pubsub_ctx_t **pb_ctx_loc,
                   int status) {
  sbp_zmq_pubsub_destroy(sbp_ctx_loc);
  sbp_zmq_pubsub_destroy(pb_ctx_loc);
  logging_deinit();
  return status;
}
