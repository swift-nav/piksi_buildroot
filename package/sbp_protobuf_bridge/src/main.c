#include <assert.h>
#include <czmq.h>
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


// static int cleanup(settings_ctx_t **settings_ctx_loc,
//                    sbp_zmq_pubsub_ctx_t **pubsub_ctx_loc,
//                    int status) {
//   sbp_zmq_pubsub_destroy(pubsub_ctx_loc);
//   settings_destroy(settings_ctx_loc);
//   logging_deinit();

//   return status;
// }


static void utc_time_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
  (void) context;
  (void) sender_id;
  (void) len;
  msg_utc_time_t *time = (msg_utc_time_t*)msg;

  piksi_log(LOG_INFO, "utc time callback");
}

int main(int argc, char *argv[])
{
  logging_init(PROGRAM_NAME);

  /* Prevent czmq from catching signals */
  zsys_handler_set(NULL);

  ctx = sbp_zmq_pubsub_create(SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    piksi_log(LOG_ERR, "settings_ctx error")
    // return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }

  if (sbp_callback_register(SBP_MSG_UTC_TIME, utc_time_callback, NULL) != 0) {
    piksi_log(LOG_ERR, "error setting UTC TIME callback");
    // return cleanup(&settings_ctx, &ctx, EXIT_FAILURE);
  }
}
