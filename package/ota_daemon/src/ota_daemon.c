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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <json-c/json.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <libpiksi/logging.h>
#include <libpiksi/runit.h>
#include <libpiksi/settings.h>
#include <libpiksi/sha256.h>
#include <libpiksi/util.h>
#include <libpiksi/version.h>

#include <libnetwork.h>

#include "ota_settings.h"

#define OTA_RESPONSE_FILE_PATH "/upgrade_data/ota/response"
#define OTA_IMAGE_FILE_PATH "/upgrade_data/ota/image"
#define OTA_IMAGE_SHA256SUM_FILE_PATH "/upgrade_data/ota/image_sha256sum"
#define OTA_DEFAULT_URL "https://upgrader.skylark.swiftnav.com/images"

/* Timeout is one hour with 15 % jitter */
#define OTA_TIMEOUT_AVG_S (3600)
#define OTA_TIMEOUT_JITTER_PERCENTAGE (15)
#define OTA_TIMEOUT_JITTER_MAX (100 + OTA_TIMEOUT_JITTER_PERCENTAGE)
#define OTA_TIMEOUT_JITTER_MIN (100 - OTA_TIMEOUT_JITTER_PERCENTAGE)

/* Timeout before first try, give some time for network connection init */
#define OTA_INITIAL_TIMEOUT_S (60)

/* Max JSON response size 4 KB */
#define OTA_ENQUIRE_MAX_BYTES (4 * 1024)
/* Max upgrade image size 30 MB */
#define OTA_DOWNLOAD_MAX_BYTES (30 * 1024 * 1024)

static inline int ota_timeout_s(void)
{
  int jitter = rand() % (OTA_TIMEOUT_JITTER_MAX + 1 - OTA_TIMEOUT_JITTER_MIN);
  float jitter_scaled = (jitter + OTA_TIMEOUT_JITTER_MIN) / 100.f;
  return round(OTA_TIMEOUT_AVG_S * jitter_scaled);
}

typedef enum ota_op_mode_e {
  OP_MODE_OTA_CLIENT = 0,
  OP_MODE_SETTINGS_DAEMON = 1,
  OP_MODE_COUNT = 2
} ota_op_mode_t;

typedef struct ota_resp_s {
  char url[4096];
  char sha256[65];
  char version[64];
} ota_resp_t;

static ota_op_mode_t op_mode = OP_MODE_COUNT;
static const char *opt_url = NULL;
static bool opt_debug = false;

static void usage(char *command)
{
  printf("Usage: %s [ --ota [--url url] | --settings ]\n", command);

  puts("\nMode selection options");
  puts("\t--ota        launch in ota client mode");
  puts("\t--settings   launch in settings monitor mode");

  puts("\nOTA mode options");
  puts("\t--url        <OTA endpoint URL>");

  puts("\nGeneral options");
  puts("\t--debug");
}

static int parse_options(int argc, char *argv[])
{
  enum {
    OPT_ID_DEBUG = 1,
    OPT_ID_MODE_OTA,
    OPT_ID_MODE_SETTINGS,
    OPT_ID_URL,
  };

  // clang-format off
  const struct option long_opts[] = {
    {"debug",     no_argument,        0, OPT_ID_DEBUG},
    {"ota",       no_argument,        0, OPT_ID_MODE_OTA},
    {"settings",  no_argument,        0, OPT_ID_MODE_SETTINGS},
    {"url",       required_argument,  0, OPT_ID_URL},
    {0, 0, 0, 0},
  };
  // clang-format on

  int opt;

  while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
    switch (opt) {
    case OPT_ID_MODE_OTA: {
      op_mode = OP_MODE_OTA_CLIENT;
    } break;

    case OPT_ID_MODE_SETTINGS: {
      op_mode = OP_MODE_SETTINGS_DAEMON;
    } break;

    case OPT_ID_URL: {
      opt_url = optarg;
    } break;

    case OPT_ID_DEBUG: {
      opt_debug = true;
    } break;

    default: {
      piksi_log(LOG_ERR, "Invalid option");
      return -1;
    } break;
    }
  }

  if (op_mode == OP_MODE_OTA_CLIENT && opt_url == NULL) {
    opt_url = OTA_DEFAULT_URL;
    piksi_log(LOG_INFO, "Using default URL: %s\n", opt_url);
  }

  return 0;
}

static void ota_sig_handler(int signum, siginfo_t *info, void *ucontext)
{
  (void)ucontext;
  piksi_log(LOG_DEBUG, "%s: received signal: %d, sender: %d", __FUNCTION__, signum, info->si_pid);
  libnetwork_shutdown(NETWORK_TYPE_OTA);
}

static int configure_libnetwork(int fd, const char *url, size_t max_bytes, network_context_t *ctx)
{
  network_status_t status = NETWORK_STATUS_SUCCESS;

  if ((status = libnetwork_set_url(ctx, url)) != NETWORK_STATUS_SUCCESS) {
    piksi_log(LOG_ERR, libnetwork_status_text(status));
    return 1;
  }

  if ((status = libnetwork_set_fd(ctx, fd)) != NETWORK_STATUS_SUCCESS) {
    piksi_log(LOG_ERR, libnetwork_status_text(status));
    return 1;
  }

  if ((status = libnetwork_set_debug(ctx, opt_debug)) != NETWORK_STATUS_SUCCESS) {
    piksi_log(LOG_ERR, libnetwork_status_text(status));
    return 1;
  }

  if ((status = libnetwork_set_continuous(ctx, false)) != NETWORK_STATUS_SUCCESS) {
    piksi_log(LOG_ERR, libnetwork_status_text(status));
    return 1;
  }

  if ((status = libnetwork_set_max_bytes(ctx, max_bytes)) != NETWORK_STATUS_SUCCESS) {
    piksi_log(LOG_ERR, libnetwork_status_text(status));
    return 1;
  }

  return 0;
}

static int ota_upgrade()
{
  int ret = 0;

  const char *upgrade_cmd =
    "sh -c 'set -o pipefail; "
    "sudo upgrade_tool --debug " OTA_IMAGE_FILE_PATH
    " && "
    "sudo /etc/init.d/do_sbp_msg_reset'";

  runit_config_t cfg = (runit_config_t){
    .service_dir = OTA_RUNIT_SERVICE_DIR,
    .service_name = "upgrade_tool",
    .command_line = upgrade_cmd,
    .finish_command = NULL,
    .restart = false,
  };

  ret = start_runit_service(&cfg);

  if (ret) {
    piksi_log(LOG_ERR | LOG_SBP, "Failed to start upgrade_tool: %d", ret);
  }

  return ret;
}

static int ota_sha256sum(const char *expected)
{
  char actual[SHA256SUM_LEN] = {0};

  if (sha256sum_file(OTA_IMAGE_FILE_PATH, actual, sizeof(actual))) {
    return -1;
  }

  if (sha256sum_cmp(actual, expected)) {
    piksi_log(LOG_ERR, "SHA256 mismatch");
    piksi_log(LOG_ERR, "SHA256 actual  : %s", actual);
    piksi_log(LOG_ERR, "SHA256 expected: %s", expected);
    return -1;
  }

  return 0;
}

/*
 * return:
 *   > 0 if offered version is newer
 *   = 0 if offered and current versions are equal
 *   < 0 if current version is newer or error
 */
static int ota_version_check(const char *offered)
{
  piksi_version_t offered_parsed = {0};
  if (version_parse_str(offered, &offered_parsed)) {
    return -1;
  }

  piksi_version_t current_parsed = {0};
  if (version_current_get(&current_parsed)) {
    return -1;
  }

  int ret = version_cmp(&offered_parsed, &current_parsed);

  if (ret > 0) {
    piksi_log(LOG_INFO, "New version available via OTA: %s", offered);
  }

  return ret;
}

static bool ota_get_json_str(const char *str_name,
                             const json_object *json_parent,
                             char *str,
                             size_t str_len)
{
  struct json_object *json_obj = json_object_object_get(json_parent, str_name);
  const char *json_str = json_object_get_string(json_obj);

  return snprintf_warn(str, str_len, "%s", json_str);
}

static bool ota_parse_response(ota_resp_t *parsed_resp)
{
  /* Read and mmap the json file */
  int fd = open(OTA_RESPONSE_FILE_PATH, O_RDONLY);
  if (fd == -1) {
    return false;
  }

  struct stat sb = {0};

  if (fstat(fd, &sb) == -1) {
    close(fd);
    return false;
  }

  void *fmap = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (fmap == MAP_FAILED) {
    return false;
  }

  /* Parse json into a json_object struct */
  struct json_object *fjson = json_tokener_parse(fmap);

  bool ret = ota_get_json_str("url", fjson, parsed_resp->url, sizeof(parsed_resp->url));
  ret &= ota_get_json_str("version", fjson, parsed_resp->version, sizeof(parsed_resp->version));
  ret &= ota_get_json_str("sha256", fjson, parsed_resp->sha256, sizeof(parsed_resp->sha256));

  /* Cleanup objects when done */
  json_object_object_del(fjson, "");

  return ret;
}

static bool ota_create_files(int *fd_resp, int *fd_img)
{
  /* Create empty files */
  *fd_resp = open(OTA_RESPONSE_FILE_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (*fd_resp < 0) {
    piksi_log(LOG_ERR | LOG_SBP, "fd error (%d) \"%s\"", errno, strerror(errno));
    return false;
  }

  *fd_img = open(OTA_IMAGE_FILE_PATH, O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (*fd_img < 0) {
    piksi_log(LOG_ERR | LOG_SBP, "fd error (%d) \"%s\"", errno, strerror(errno));
    return false;
  }

  return true;
}

static void ota_close_files(int *fd_resp, int *fd_img)
{
  if (*fd_resp >= 0) {
    close(*fd_resp);
    *fd_resp = -1;
  }

  if (*fd_img >= 0) {
    close(*fd_img);
    *fd_img = -1;
  }
}

static bool ota_client_loop(void)
{
  bool ret = true;

  setup_sigterm_handler(ota_sig_handler);
  setup_sigint_handler(ota_sig_handler);

  /* For OTA_TIMEOUT_S jitter */
  srand(time(NULL));

  sigwait_params_t params = {0};
  setup_sigtimedwait(&params, SIGUSR1, OTA_INITIAL_TIMEOUT_S);

  network_context_t *nw_ctx = libnetwork_create(NETWORK_TYPE_OTA);

  int fd_resp = -1;
  int fd_img = -1;

  while (!libnetwork_shutdown_signaled(nw_ctx) && run_sigtimedwait(&params)) {
    /* Update timeout jitter */
    update_sigtimedwait(&params, ota_timeout_s());

    piksi_log(LOG_INFO, "Checking FW update, next timeout %d s...", params.timeout.tv_sec);

    if (!ota_create_files(&fd_resp, &fd_img)) {
      ret = false;
      break;
    }

    if (configure_libnetwork(fd_resp, opt_url, OTA_ENQUIRE_MAX_BYTES, nw_ctx)) {
      ret = false;
      break;
    }

    if (!ota_enquire(nw_ctx)) {
      ota_close_files(&fd_resp, &fd_img);
      continue;
    }

    ota_resp_t parsed_resp = {0};
    if (!ota_parse_response(&parsed_resp)) {
      ota_close_files(&fd_resp, &fd_img);
      continue;
    }

    if (ota_version_check(parsed_resp.version) <= 0) {
      ota_close_files(&fd_resp, &fd_img);
      continue;
    }

    if (configure_libnetwork(fd_img, parsed_resp.url, OTA_DOWNLOAD_MAX_BYTES, nw_ctx)) {
      ret = false;
      break;
    }

    if (!ota_download(nw_ctx)) {
      ota_close_files(&fd_resp, &fd_img);
      continue;
    }

    ota_close_files(&fd_resp, &fd_img);

    if (ota_sha256sum(parsed_resp.sha256)) {
      continue;
    }

    ota_upgrade();
  }

  ota_close_files(&fd_resp, &fd_img);
  libnetwork_destroy(&nw_ctx);

  return ret;
}

int main(int argc, char *argv[])
{
  logging_init(OTA_RUNIT_SERVICE_NAME);

  if (parse_options(argc, argv) != 0) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  bool ret = false;

  switch (op_mode) {
  case OP_MODE_OTA_CLIENT: ret = ota_client_loop(); break;
  case OP_MODE_SETTINGS_DAEMON: ret = settings_loop_simple(ota_settings); break;
  case OP_MODE_COUNT:
  default: ret = false;
  }

  logging_deinit();

  if (!ret) {
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
