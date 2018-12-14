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

#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include <libpiksi/logging.h>
#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/loop.h>
#include <libpiksi/settings_client.h>
#include <libpiksi/util.h>
#include <libpiksi/serial_utils.h>

#include <libsbp/sbp.h>
#include <libsbp/piksi.h>
#include <libsbp/system.h>

#define PROGRAM_NAME "csac_daemon"

#define SBP_SUB_ENDPOINT "ipc:///var/run/sockets/internal.pub" /* SBP Internal Out */
#define SBP_PUB_ENDPOINT "ipc:///var/run/sockets/internal.sub" /* SBP Internal In */

#define METRICS_NAME "csac_daemon"
#define SETTINGS_METRICS_NAME ("csac/" METRICS_NAME)

#define SBP_FRAMING_MAX_PAYLOAD_SIZE (255u)
#define CSAC_TELEM_UPDATE_INTERVAL (1000u)
#define HEADER_SEND_DIVISOR (10)
#define POLL_SLEEP_MICROSECONDS (100000)
#define POLL_RETRIES (3)

#define TELEM_POLL_STRING "^\r\n"
#define POLL_STR_LEN (3)
#define HEADER_POLL_STRING "6\r\n"
#define HEADER_STR_LEN (3)
#define MIN_RETURN_STR_LEN (50)

/*Header string looks like "Status,Alarm,SN,Mode,Contrast,LaserI,TCXO,HeatP,Sig,Temp,
Steer,ATune,Phase,DiscOK,TOD,LTime,Ver" in CSAC firmware version v1.10 */

#define HEADER_FIRST_CHAR 'S'
#define HEADER_LAST_CHAR 'r'


u8 *port_name = NULL;
u8 *command_string = NULL;
bool csac_telemetry_enabled = false;

struct csac_ctx_s {
  sbp_pubsub_ctx_t *sbp_ctx;
  serial_port_t *port;
  char csac_telem[SBP_FRAMING_MAX_PAYLOAD_SIZE];
  char csac_header[SBP_FRAMING_MAX_PAYLOAD_SIZE];
};

static bool csac_daemon_enabled()
{
  return csac_telemetry_enabled;
}

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMain options");
  puts("\t--serial-port <serial port for csac daemon>");
}

static int parse_options(int argc, char *argv[])
{
  enum { OPT_ID_SERIAL_PORT = 1, OPT_ID_AT_COMMAND = 2 };

  const struct option long_opts[] = {
    {"serial-port", required_argument, 0, OPT_ID_SERIAL_PORT},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_SERIAL_PORT: {
      port_name = (u8 *)optarg;
    } break;

    default: {
      puts("Invalid option");
      return -1;
    } break;
    }
  }

  if (port_name == NULL) {
    puts("Missing port");
    return -1;
  }

  return 0;
}

static int read_until_cr_lf(int fd, char *outbuf, size_t maxlen)
{
  int nbytes;   /* Number of bytes read */
  char *bufptr; /* Current char in buffer */
  bufptr = outbuf;
  char *eob = outbuf + maxlen;
  while ((nbytes = read(fd, bufptr, (size_t)(eob - bufptr) - 1)) > 0) {
    bufptr += nbytes;
    if ((size_t)(bufptr - outbuf) > 2) {
      if (bufptr[-1] == '\n' || bufptr[-1] == '\r') {
        break;
      }
    }
  }
  *bufptr = '\0';
  return (bufptr - outbuf);
}

/**
 * @brief send_csac_telem
 *
 * Sends csac telemetry string as SBP message to external endpoint
 * @param csac_telem: pointer to null_terminated c string
 */
static void send_csac_telem(struct csac_ctx_s *ctx)
{
  static int count;
  size_t telem_string_len = 0;
  /* At most the telemetry string can be SBP_FRAMING_MAX_PAYLOAD_SIZE - 1 since
   * we have to save one byte for the "id" field */
  telem_string_len =
    SWFT_MIN(strnlen(ctx->csac_telem, sizeof(ctx->csac_telem)), SBP_FRAMING_MAX_PAYLOAD_SIZE - 1);
  msg_csac_telemetry_t *msg = alloca(SBP_FRAMING_MAX_PAYLOAD_SIZE);
  memcpy(msg->telemetry, ctx->csac_telem, telem_string_len);
  msg->id = 0;
  msg->telemetry[telem_string_len] = '\0'; /* Ensure null termination */
  sbp_tx_send(sbp_pubsub_tx_ctx_get(ctx->sbp_ctx),
              SBP_MSG_CSAC_TELEMETRY,
              (u8)SWFT_MIN(telem_string_len + 1,
                           SBP_FRAMING_MAX_PAYLOAD_SIZE), /* length is strnlen + 1 for id */
              (u8 *)msg);
  count++;
  if ((count % HEADER_SEND_DIVISOR) == 0) {
    telem_string_len = SWFT_MIN(strnlen(ctx->csac_header, sizeof(ctx->csac_header)),
                                SBP_FRAMING_MAX_PAYLOAD_SIZE - 1);
    memcpy(msg->telemetry, ctx->csac_header, telem_string_len);
    msg->id = 0;
    msg->telemetry[telem_string_len] = '\0'; /* Ensure null termination */
    sbp_tx_send(sbp_pubsub_tx_ctx_get(ctx->sbp_ctx),
                SBP_MSG_CSAC_TELEMETRY_LABELS,
                (u8)SWFT_MIN(telem_string_len + 1,
                             SBP_FRAMING_MAX_PAYLOAD_SIZE), /* length is strnlen + 1 for id */
                (u8 *)msg);
  }
}


static int get_csac_telem(struct csac_ctx_s *ctx)
{
  static bool headers_received = false;
  if (!headers_received) {
    for (int tries = 0; tries < POLL_RETRIES && !headers_received; tries++) {
      memset(ctx->csac_header, 0, sizeof(ctx->csac_header));
      if (write(ctx->port->fd, HEADER_POLL_STRING, HEADER_STR_LEN) < HEADER_STR_LEN) {
        piksi_log(LOG_DEBUG, "Less than 3 bytes could be written to serial port");
        continue;
      }
      usleep(POLL_SLEEP_MICROSECONDS);
      int bytes = read_until_cr_lf(ctx->port->fd, ctx->csac_header, sizeof(ctx->csac_header));
      /* Check for expected length and expected first and last chars*/
      if (bytes > MIN_RETURN_STR_LEN) {
        /*Last character of buffer is 3 characters from end as it should end in r\r\n*/
        if (ctx->csac_header[0] == HEADER_FIRST_CHAR
            && ctx->csac_header[bytes - 3] == HEADER_LAST_CHAR) {
          headers_received = true;
        }
      }
    } /* end for (int tries ... */
  }   /* end if !(headers_received) */
  for (int tries = 0; tries < POLL_RETRIES; tries++) {
    memset(ctx->csac_telem, 0, sizeof(ctx->csac_telem));
    /* send query telemetry command followed by a CR LF */
    if (write(ctx->port->fd, TELEM_POLL_STRING, POLL_STR_LEN) < POLL_STR_LEN) {
      piksi_log(LOG_DEBUG, "Less than 3 bytes could be written to serial port");
      continue;
    }
    usleep(POLL_SLEEP_MICROSECONDS);
    int bytes = read_until_cr_lf(ctx->port->fd, ctx->csac_telem, sizeof(ctx->csac_telem));
    if (bytes > MIN_RETURN_STR_LEN && headers_received) {
      return 0;
    }
  }
  return 1; /* either we haven't yet gotten the headers or we didn't get a long
               enough telemetry string */
}

/**
 * @brief csac_telem_callback - used to trigger csac telemetry pollling
 */
static void csac_telem_callback(pk_loop_t *loop, void *timer_handle, int status, void *context)
{
  (void)loop;
  (void)timer_handle;
  (void)status;
  struct csac_ctx_s *csac_ctx = (struct csac_ctx_s *)context;

  if (csac_daemon_enabled()) {
    if (get_csac_telem(csac_ctx) == 0) {
      send_csac_telem(csac_ctx);
    } else {
      piksi_log(
        LOG_WARNING | LOG_SBP,
        "Could not retrieve CSAC telemetry. Ensure CSAC is connected and UART0 mode is disabled.");
    }
  }
}

static int settings_changed(void *context)
{
  struct csac_ctx_s *csac_ctx = (struct csac_ctx_s *)context;
  if (!csac_telemetry_enabled) {
    piksi_log(LOG_DEBUG, "CSAC telmetry disabled");
    if (serial_port_is_open(csac_ctx->port)) {
      serial_port_close(csac_ctx->port);
    }
  } else {
    piksi_log(LOG_DEBUG, "CSAC telmetry enabled");
    if (!serial_port_is_open(csac_ctx->port)) {
      serial_port_open_baud(csac_ctx->port, B57600);
    }
    if (serial_port_is_open(csac_ctx->port)) {
      return 0;
    } else {
      piksi_log(LOG_ERR | LOG_SBP,
                "Unable to open UART0 for CSAC telmetry. CSAC telemetry will remain disabled.");
      return 1;
    }
  }
  return 0;
}

static int cleanup(pk_loop_t **pk_loop_loc,
                   pk_settings_ctx_t **settings_ctx_loc,
                   sbp_pubsub_ctx_t **pubsub_ctx_loc,
                   serial_port_t **port_loc,
                   int status)
{
  pk_loop_destroy(pk_loop_loc);
  if (*pubsub_ctx_loc != NULL) {
    sbp_pubsub_destroy(pubsub_ctx_loc);
  }
  pk_settings_destroy(settings_ctx_loc);
  serial_port_destroy(port_loc);
  logging_deinit();

  return status;
}

int main(int argc, char *argv[])
{
  pk_loop_t *loop = NULL;
  pk_settings_ctx_t *settings_ctx = NULL;
  sbp_pubsub_ctx_t *ctx = NULL;
  serial_port_t *port = NULL;
  struct csac_ctx_s csac_ctx = {.sbp_ctx = NULL, .port = NULL};

  logging_init(PROGRAM_NAME);

  if (parse_options(argc, argv) != 0) {
    piksi_log(LOG_ERR, "invalid arguments");
    usage(argv[0]);
    exit(cleanup(&loop, &settings_ctx, &ctx, &port, EXIT_FAILURE));
  }

  port = serial_port_create((char *)port_name);
  if (port == NULL) {
    exit(cleanup(&loop, &settings_ctx, &ctx, &port, EXIT_FAILURE));
  }
  csac_ctx.port = port;

  loop = pk_loop_create();
  if (loop == NULL) {
    exit(cleanup(&loop, &settings_ctx, &ctx, &port, EXIT_FAILURE));
  }

  ctx = sbp_pubsub_create(METRICS_NAME, SBP_PUB_ENDPOINT, SBP_SUB_ENDPOINT);
  if (ctx == NULL) {
    exit(cleanup(&loop, &settings_ctx, &ctx, &port, EXIT_FAILURE));
  }
  csac_ctx.sbp_ctx = ctx;

  if (pk_loop_timer_add(loop, CSAC_TELEM_UPDATE_INTERVAL, csac_telem_callback, &csac_ctx) == NULL) {
    exit(cleanup(&loop, &settings_ctx, &ctx, &port, EXIT_FAILURE));
  }

  settings_ctx = pk_settings_create(SETTINGS_METRICS_NAME);

  if (settings_ctx == NULL) {
    piksi_log(LOG_ERR, "Error registering for settings!");
    exit(cleanup(&loop, &settings_ctx, &ctx, &port, EXIT_FAILURE));
  }

  if (sbp_rx_attach(sbp_pubsub_rx_ctx_get(ctx), loop) != 0) {
    exit(cleanup(&loop, &settings_ctx, &ctx, &port, EXIT_FAILURE));
  }

  if (pk_settings_attach(settings_ctx, loop) != 0) {
    piksi_log(LOG_ERR, "Error registering for settings read!");
    exit(cleanup(&loop, &settings_ctx, &ctx, &port, EXIT_FAILURE));
  }

  pk_settings_register(settings_ctx,
                       "csac",
                       "telemetry_enabled",
                       &csac_telemetry_enabled,
                       sizeof(csac_telemetry_enabled),
                       SETTINGS_TYPE_BOOL,
                       settings_changed,
                       &csac_ctx);

  pk_loop_run_simple(loop);

  exit(cleanup(&loop, &settings_ctx, &ctx, &port, EXIT_SUCCESS));
}
