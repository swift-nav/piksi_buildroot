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

#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libpiksi/logging.h>

#define AGENT_TYPE "NTRIP ntrip-client/1.0"

static bool debug = false;
static const char *fifo_file_path = NULL;
static const char *url = NULL;

typedef struct {
  int fd;
} ntrip_config_t;

static int ntrip_request(CURL *curl)
{
  char error_buf[CURL_ERROR_SIZE];
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buf);

  while (true) {
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      if (debug) {
        piksi_log(LOG_DEBUG, "curl error (%d) \"%s\"\n", res, error_buf);
      }
    }

    usleep(1000000);
  }

  return 0;
}

static size_t ntrip_download_callback(char *buf, size_t size, size_t n, void *data)
{
  const ntrip_config_t * const config = data;

  ssize_t ret = write(config->fd, buf, size * n);

  if (debug) {
    piksi_log(LOG_DEBUG, "write bytes (%d) %d\n", size * n, ret);
  }

  return ret;
}

static int ntrip_setup(void)
{
  CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
  if (res != CURLE_OK) {
    return -1;
  }

  return 0;
}

static void ntrip_teardown(void)
{
  curl_global_cleanup();
}

static int ntrip_download(const ntrip_config_t * const config)
{
  CURL *curl = curl_easy_init();
  if (curl == NULL) {
    return -1;
  }


  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
  chunk = curl_slist_append(chunk, "Ntrip-Version: Ntrip/2.0");

  curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    chunk);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,     AGENT_TYPE);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ntrip_download_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA,     config);
  curl_easy_setopt(curl, CURLOPT_URL,           url);

  int ret = ntrip_request(curl);
  curl_easy_cleanup(curl);

  return ret;
}

static void usage(char *command)
{
  printf("Usage: %s\n", command);

  puts("\nMain options");
  puts("\t--file <file>");
  puts("\t--url <url>");

  puts("\nMisc options");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_FILE = 1,
    OPT_ID_URL,
    OPT_ID_DEBUG,
  };

  const struct option long_opts[] = {
    {"file",  required_argument, 0, OPT_ID_FILE},
    {"url  ", required_argument, 0, OPT_ID_URL},
    {"debug", no_argument,       0, OPT_ID_DEBUG},
    {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
      case OPT_ID_FILE: {
        fifo_file_path = optarg;
      }
      break;

      case OPT_ID_URL: {
        url = optarg;
      }
      break;

      case OPT_ID_DEBUG: {
        debug = true;
      }
      break;

      default: {
        puts("Invalid option");
        return -1;
      }
      break;
    }
  }

  if (fifo_file_path == NULL) {
    puts("Missing file");
    return -1;
  }

  if (url == NULL) {
    puts("Missing url");
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[])
{
  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  int fd = open(fifo_file_path, O_WRONLY);
  if (fd < 0) {
    piksi_log(LOG_ERR, "fifo error (%d) \"%s\"", errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  int ret = ntrip_setup();
  if (ret != 0) {
    piksi_log(LOG_ERR, "setup error");
    close(fd);
    exit(EXIT_FAILURE);
  }

  ntrip_config_t config = {.fd = fd};

  ret = ntrip_download(&config);
  if (ret != 0) {
    piksi_log(LOG_ERR, "request error");
    ntrip_teardown();
    close(fd);
    exit(EXIT_FAILURE);
  }

  ntrip_teardown();
  close(fd);

  return EXIT_SUCCESS;
}
