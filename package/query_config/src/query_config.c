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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <libpiksi/min_ini.h>

#define SETTINGS_FILE "/persistent/config.ini"
#define BUFSIZE 256

static struct {
  const char* filename;
  const char* section;
  const char* key;
} options = {
  .filename = NULL,
  .section = NULL,
  .key = NULL,
};

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\t--file       the file to load");
  puts("\t--section    the section to query");
  puts("\t--key        the key to load");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_FILE = 1,
    OPT_ID_SECTION,
    OPT_ID_KEY,
  };

  // clang-format off
  const struct option long_opts[] = {
    {"file",      required_argument, 0, OPT_ID_FILE},
    {"section",   required_argument, 0, OPT_ID_SECTION},
    {"key",       required_argument, 0, OPT_ID_KEY},
    {0, 0, 0, 0},
  };
  // clang-format on

  int opt;

  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {

      case OPT_ID_FILE:    { options.filename = optarg; } break;
      case OPT_ID_SECTION: { options.section = optarg;  } break;
      case OPT_ID_KEY:     { options.key = optarg;      } break;

      default:             { puts("Invalid option");    } return -1;
    }
  }

  if (options.filename == NULL)  {
    options.filename = SETTINGS_FILE;
  }

  if (options.section == NULL)  {
    puts("Missing section name");
    return -1;
  }

  if (options.key == NULL)  {
    puts("Missing key name");
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[]) {

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  int status = EXIT_SUCCESS;

  const char* default_value = "{2F9D26FF-F64C-4F9F-94FE-AE9F57758835}";
  char buf[BUFSIZE];

  ini_gets(options.section, options.key, default_value, buf, sizeof(buf), options.filename);

  if (strcmp(buf, default_value) == 0) {
    status = EXIT_FAILURE;
  } else {
    puts(buf);
  }

  return status;
}

