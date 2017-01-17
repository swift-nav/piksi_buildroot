/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Gareth McMullin <gareth@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sbp_zmq.h>
#include <getopt.h>

#include "sbp_fileio.h"

static const char *pub_endpoint = NULL;
static const char *sub_endpoint = NULL;

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("-p, --pub <addr>");
  puts("-s, --sub <addr>");
}

static int parse_options(int argc, char *argv[])
{
  const struct option long_opts[] = {
    {"pub", required_argument, 0, 'p'},
    {"sub", required_argument, 0, 's'},
    {0, 0, 0, 0}
  };

  int c;
  int opt_index;
  while ((c = getopt_long(argc, argv, "p:s:",
                          long_opts, &opt_index)) != -1) {
    switch (c) {

      case 'p': {
        pub_endpoint = optarg;
      }
      break;

      case 's': {
        sub_endpoint = optarg;
      }
      break;


      default: {
        printf("invalid option\n");
        return -1;
      }
      break;
    }
  }

  if ((pub_endpoint == NULL) || (sub_endpoint == NULL)) {
    printf("ZMQ endpoints not specified\n");
    return -1;
  }

  return 0;
}

static int file_read_string(const char *filename, char *str, size_t str_size)
{
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("error opening %s\n", filename);
    return -1;
  }

  bool success = (fgets(str, str_size, fp) != NULL);

  fclose(fp);

  if (!success) {
    printf("error reading %s\n", filename);
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(1);
  }

  /* Set up SBP ZMQ */
  u16 sbp_sender_id = SBP_SENDER_ID;
  char sbp_sender_id_string[32];
  if (file_read_string("/cfg/sbp_sender_id", sbp_sender_id_string,
                        sizeof(sbp_sender_id_string)) == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }
  sbp_zmq_config_t sbp_zmq_config = {
    .sbp_sender_id = sbp_sender_id,
    .pub_endpoint = pub_endpoint,
    .sub_endpoint = sub_endpoint
  };

  sbp_zmq_state_t sbp_zmq_state;
  if (sbp_zmq_init(&sbp_zmq_state, &sbp_zmq_config) != 0) {
    exit(EXIT_FAILURE);
  }

  sbp_fileio_setup(&sbp_zmq_state);

  sbp_zmq_loop(&sbp_zmq_state);

  sbp_zmq_deinit(&sbp_zmq_state);
  exit(EXIT_SUCCESS);
}
