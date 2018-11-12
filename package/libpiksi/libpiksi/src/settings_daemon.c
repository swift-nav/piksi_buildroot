/*
 * Copyright (C) 2018 Swift Navigation Inc.
 * Contact: Swift Navigation <dev@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

/**
 * @file settings.c
 * @brief Implementation of Settings Client APIs
 *
 * The piksi settings daemon acts as the manager for onboard settings
 * registration and read responses for read requests while individual
 * processes are responsible for the ownership of settings values and
 * must respond to write requests with the value of the setting as a
 * response to the question of whether or not a write request was
 * valid and accepted by that process.
 *
 * This module provides a context and APIs for client interactions
 * with the settings manager. Where previously these APIs were intended
 * only for settings owning processes to register settings and respond
 * to write requests, the intention will be to allow a fully formed
 * settings client to be built upon these APIs to include read requests
 * and other client side queries.
 *
 * The high level approach to the client is to hold a list of unique
 * settings that can be configured as owned or non-owned (watch-only)
 * by the process running the client. Each setting which is added to
 * the list will be kept in sync with the settings daemon and/or the
 * owning process via asynchronous messages received in the routed
 * endpoint for the client.
 *
 * Standard usage is as follow, initialize the settings context:
 * \code{.c}
 * // Create the settings context
 * sd_ctx_t *sd_ctx = sd_create();
 * \endcode
 * Add a reader to the main pk_loop (if applicable)
 * \code{.c}
 * // Depending on your implementation this will vary
 * sd_attach(sd_ctx, loop);
 * \endcode
 * For settings owners, a setting is registered as follows:
 * \code{.c}
 * settings(sd_ctx, "sample_process", "sample_setting",
                     &sample_setting_data, sizeof(sample_setting_data),
                     SETTINGS_TYPE_BOOL,
                     optional_notify_callback, optional_callback_data);
 * \endcode
 * For a process that is tracking a non-owned setting, the process is similar:
 * \code{.c}
 * sd_register_watch(sd_ctx, "sample_process", "sample_setting",
                      &sample_setting_data, sizeof(sample_setting_data),
                      SETTINGS_TYPE_BOOL,
                      optional_notify_callback, optional_callback_data);
 * \endcode
 * The main difference here is that an owned setting will response to write
 * requests only, while a watch-only setting is updated on write responses
 * to stay in sync with successful updates as reported by settings owners.
 * @version v1.4
 * @date 2018-02-23
 */

#include <errno.h>
#include <unistd.h>

#include <sys/stat.h>

#include <libpiksi/sbp_pubsub.h>
#include <libpiksi/util.h>
#include <libpiksi/logging.h>
#include <libpiksi/settings_daemon.h>

#include <libsbp/settings.h>

#include <libsettings/settings.h>

#define PUB_ENDPOINT "ipc:///var/run/sockets/settings_client.sub"
#define SUB_ENDPOINT "ipc:///var/run/sockets/settings_client.pub"

typedef struct {
  pk_endpoint_t *cmd_ept;
  const char *command;
  handle_command_fn handler;
} control_command_t;

/**
 * @brief Settings Context
 *
 * This is the main context for managing client interactions with
 * the settings manager. It implements the client messaging context
 * as well as the list of types and settings necessary to perform
 * the registration, watching and callback functionality of the client.
 */
struct sd_ctx_s {
  pk_loop_t *loop;
  sbp_pubsub_ctx_t *pubsub_ctx;
  settings_t *settings;
};

static sd_term_fn sd_term_handler = NULL;
static sd_child_fn sd_child_handler = NULL;

static void destroy(sd_ctx_t **ctx)
{
  free(*ctx);
  *ctx = NULL;
}

static int send_wrap(void *ctx, uint16_t msg_type, uint8_t len, uint8_t *payload)
{
  sd_ctx_t *sd_ctx = (sd_ctx_t *)ctx;
  return sbp_tx_send(sbp_pubsub_tx_ctx_get(sd_ctx->pubsub_ctx), msg_type, len, payload);
}

static int send_from_wrap(void *ctx,
                          uint16_t msg_type,
                          uint8_t len,
                          uint8_t *payload,
                          uint16_t sbp_sender_id)
{
  sd_ctx_t *sd_ctx = (sd_ctx_t *)ctx;
  return sbp_tx_send_from(sbp_pubsub_tx_ctx_get(sd_ctx->pubsub_ctx),
                          msg_type,
                          len,
                          payload,
                          sbp_sender_id);
}

static int wait_wrap(void *ctx, int timeout_ms)
{
  sd_ctx_t *sd_ctx = (sd_ctx_t *)ctx;

  return pk_loop_run_simple_with_timeout(sd_ctx->loop, timeout_ms);
}

static void signal_wrap(void *ctx)
{
  sd_ctx_t *sd_ctx = (sd_ctx_t *)ctx;
  sbp_rx_reader_interrupt(sbp_pubsub_rx_ctx_get(sd_ctx->pubsub_ctx));
}

static int reg_cb_wrap(void *ctx,
                       uint16_t msg_type,
                       sbp_msg_callback_t cb,
                       void *cb_context,
                       sbp_msg_callbacks_node_t **node)
{
  sd_ctx_t *sd_ctx = (sd_ctx_t *)ctx;
  sbp_rx_callback_register(sbp_pubsub_rx_ctx_get(sd_ctx->pubsub_ctx),
                           msg_type,
                           cb,
                           cb_context,
                           node);
}

static int unreg_cb_wrap(void *ctx, sbp_msg_callbacks_node_t **node)
{
  sd_ctx_t *sd_ctx = (sd_ctx_t *)ctx;
  return sbp_rx_callback_remove(sbp_pubsub_rx_ctx_get(sd_ctx->pubsub_ctx), node);
}

<<<<<<< 6802ba4b3aa224f66a7220af7463e95b413c102e:package/libpiksi/libpiksi/src/settings.c
settings_ctx_t *settings_create(const char *ident)
=======
sd_ctx_t *sd_create(void)
>>>>>>> Settings naming rework:package/libpiksi/libpiksi/src/settings_daemon.c
{
  sd_ctx_t *ctx = (sd_ctx_t *)malloc(sizeof(*ctx));
  if (ctx == NULL) {
    piksi_log(LOG_ERR, "error allocating context");
    return ctx;
  }

  ctx->loop = NULL;

  ctx->pubsub_ctx = sbp_pubsub_create(ident, PUB_ENDPOINT, SUB_ENDPOINT);
  if (ctx->pubsub_ctx == NULL) {
    piksi_log(LOG_ERR, "error creating PUBSUB context");
    destroy(&ctx);
    return ctx;
  }

  uint16_t id = sbp_sender_id_get();

  settings_api_t api = {
    api.ctx = (void *)ctx,
    api.send = send_wrap,
    api.send_from = send_from_wrap,
    api.wait_init = NULL,
    api.wait = wait_wrap,
    api.wait_deinit = NULL,
    api.signal = signal_wrap,
    api.register_cb = reg_cb_wrap,
    api.unregister_cb = unreg_cb_wrap,
    api.log = piksi_log,
  };

  ctx->settings = settings_create(id, &api);

  return ctx;
}

void sd_destroy(sd_ctx_t **ctx)
{
  if (ctx == NULL || *ctx == NULL) {
    return;
  }

  settings_destroy(&(*ctx)->settings);

  destroy(ctx);
}

int sd_register_enum(sd_ctx_t *ctx, const char *const enum_names[], settings_type_t *type)
{
  assert(ctx != NULL);
  assert(enum_names != NULL);
  assert(type != NULL);

  return settings_register_enum(ctx->settings, enum_names, type);
}

int sd_register(sd_ctx_t *ctx,
                const char *section,
                const char *name,
                void *var,
                size_t var_len,
                settings_type_t type,
                settings_notify_fn notify,
                void *notify_context)
{
  return settings_register_setting(ctx->settings,
                                   section,
                                   name,
                                   var,
                                   var_len,
                                   type,
                                   notify,
                                   notify_context);
}

int sd_register_readonly(sd_ctx_t *ctx,
                         const char *section,
                         const char *name,
                         const void *var,
                         size_t var_len,
                         settings_type_t type)
{
  return settings_register_readonly(ctx->settings, section, name, (void *)var, var_len, type);
}

int sd_register_watch(sd_ctx_t *ctx,
                      const char *section,
                      const char *name,
                      void *var,
                      size_t var_len,
                      settings_type_t type,
                      settings_notify_fn notify,
                      void *notify_context)
{
  return settings_register_watch(ctx->settings,
                                 section,
                                 name,
                                 var,
                                 var_len,
                                 type,
                                 notify,
                                 notify_context);
}

int sd_attach(sd_ctx_t *ctx, pk_loop_t *pk_loop)
{
  assert(ctx != NULL);
  assert(pk_loop != NULL);

  ctx->loop = pk_loop;

  return sbp_rx_attach(sbp_pubsub_rx_ctx_get(ctx->pubsub_ctx), pk_loop);
}

static void signal_handler_extended(int signum, siginfo_t *info, void *ucontext)
{
  (void)ucontext;

  if (signum == SIGINT || signum == SIGTERM) {

    if (info == NULL) {
      piksi_log(LOG_DEBUG, "%s: caught signal: %d", __FUNCTION__, signum);
    } else {
      piksi_log(LOG_DEBUG,
                "%s: caught signal: %d (sender: %d)",
                __FUNCTION__,
                signum,
                info->si_pid);
    }

    if (sd_term_handler != NULL) {
      sd_term_handler();
    }

    exit(EXIT_SUCCESS);

  } else if (signum == SIGCHLD) {

    if (sd_child_handler != NULL) {
      sd_child_handler();
    }
  }
}

static void signal_handler(int signum)
{
  signal_handler_extended(signum, (siginfo_t *)NULL, NULL);
}

static void setup_signal_handlers()
{
  setup_sigint_handler(signal_handler_extended);
  setup_sigterm_handler(signal_handler_extended);
  setup_sigchld_handler(signal_handler);
}

static int command_receive_callback(const u8 *data, const size_t length, void *context)
{
  u8 *result = (u8 *)context;

  if (length != 1) {
    piksi_log(LOG_WARNING, "command received had invalid length: %u", length);
  } else {
    *result = data[0];
  }

  return 0;
}

static void control_handler(pk_loop_t *loop, void *handle, int status, void *context)
{
  (void)loop;
  (void)status;

  control_command_t *cmd_info = (control_command_t *)context;

  u8 data = 0;
  if (pk_endpoint_receive(cmd_info->cmd_ept, command_receive_callback, &data) != 0) {
    piksi_log(LOG_ERR,
              "%s: error in %s (%s:%d): %s",
              __FUNCTION__,
              "pk_endpoint_receive",
              __FILE__,
              __LINE__,
              pk_endpoint_strerror());
    return;
  }

  if (data != cmd_info->command[0]) {
    piksi_log(LOG_WARNING, "command had invalid value: %c", data);
    return;
  }

  u8 result = cmd_info->handler() ? 1 : 0;

  pk_endpoint_send(cmd_info->cmd_ept, &result, 1);
}

static const char *get_socket_ident(const char *metrics_ident, const char *usage)
{
  static char buffer[128] = {0};
  snprintf(buffer, sizeof(buffer), "%s/%s", metrics_ident, usage);

  return buffer;
}

static bool configure_control_socket(const char *metrics_ident,
                                     pk_loop_t *loop,
                                     const char *control_socket,
                                     const char *control_socket_file,
                                     const char *control_command,
                                     handle_command_fn do_handle_command,
                                     pk_endpoint_t **rep_socket,
                                     control_command_t **ctrl_command_info)
{
#define CHECK_PRECONDITION(X)                                                                     \
  if (!(X)) {                                                                                     \
    piksi_log(LOG_ERR, "Precondition check failed: %s (%s:%d)", __STRING(X), __FILE__, __LINE__); \
    return false;                                                                                 \
  }

  CHECK_PRECONDITION(loop != NULL);
  CHECK_PRECONDITION(control_socket != NULL);
  CHECK_PRECONDITION(control_socket_file != NULL);
  CHECK_PRECONDITION(control_command != NULL);
  CHECK_PRECONDITION(control_handler != NULL);
  CHECK_PRECONDITION(do_handle_command != NULL);
  CHECK_PRECONDITION(ctrl_command_info != NULL);

  CHECK_PRECONDITION(rep_socket != NULL);

#undef CHECK_PRECONDITION

  int rc = chmod(control_socket_file, 0777);
  if (rc != 0) {
    piksi_log(LOG_ERR, "Error configuring IPC pipe permissions: %s", strerror(errno));
    return false;
  }

  *rep_socket = pk_endpoint_create(pk_endpoint_config()
                                     .endpoint(control_socket)
                                     .identity(get_socket_ident(metrics_ident, "control/rep"))
                                     .type(PK_ENDPOINT_REP)
                                     .get());
  if (*rep_socket == NULL) {
    const char *err_msg = pk_endpoint_strerror();
    piksi_log(LOG_ERR, "Error creating IPC control path: %s, error: %s", control_socket, err_msg);
    return false;
  }

  if (pk_endpoint_loop_add(*rep_socket, loop) < 0) {
    piksi_log(LOG_ERR, "Error adding IPC socket to loop: %s", control_socket);
    return false;
  }

  *ctrl_command_info = malloc(sizeof(control_command_t));

  **ctrl_command_info = (control_command_t){
    .cmd_ept = *rep_socket,
    .command = control_command,
    .handler = do_handle_command,
  };

  if (pk_loop_endpoint_reader_add(loop, *rep_socket, control_handler, *ctrl_command_info) == NULL) {
    piksi_log(LOG_ERR, "Error registering reader for control handler");
    return false;
  }

  return true;
}

<<<<<<< 6802ba4b3aa224f66a7220af7463e95b413c102e:package/libpiksi/libpiksi/src/settings.c
bool settings_loop(const char *metrics_ident,
                   const char *control_socket,
                   const char *control_socket_file,
                   const char *control_command,
                   register_settings_fn do_settings_register,
                   handle_command_fn do_handle_command,
                   settings_term_fn do_handle_term,
                   settings_child_fn do_handle_child)
=======
bool sd_loop(const char *control_socket,
             const char *control_socket_file,
             const char *control_command,
             register_sd_fn do_sd_register,
             handle_command_fn do_handle_command,
             sd_term_fn do_handle_term,
             sd_child_fn do_handle_child)
>>>>>>> Settings naming rework:package/libpiksi/libpiksi/src/settings_daemon.c
{
  piksi_log(LOG_INFO, "Starting daemon mode for settings...");

  sd_term_handler = do_handle_term;
  sd_child_handler = do_handle_child;

  pk_loop_t *loop = pk_loop_create();
  if (loop == NULL) {
    goto sd_loop_cleanup;
  }

  /* Install our own signal handlers */
  setup_signal_handlers();

  pk_endpoint_t *rep_socket = NULL;
  control_command_t *cmd_info = NULL;

  bool ret = true;

  /* Set up settings */
<<<<<<< 6802ba4b3aa224f66a7220af7463e95b413c102e:package/libpiksi/libpiksi/src/settings.c
  settings_ctx_t *settings_ctx = settings_create(metrics_ident);
  if (settings_ctx == NULL) {
=======
  sd_ctx_t *sd_ctx = sd_create();
  if (sd_ctx == NULL) {
>>>>>>> Settings naming rework:package/libpiksi/libpiksi/src/settings_daemon.c
    ret = false;
    goto sd_loop_cleanup;
  }

  if (sd_attach(sd_ctx, loop) != 0) {
    ret = false;
    goto sd_loop_cleanup;
  }

  do_sd_register(sd_ctx);

  if (control_socket != NULL) {
    bool control_sock_configured = configure_control_socket(metrics_ident,
                                                            loop,
                                                            control_socket,
                                                            control_socket_file,
                                                            control_command,
                                                            do_handle_command,
                                                            &rep_socket,
                                                            &cmd_info);
    if (!control_sock_configured) {
      ret = false;
      goto sd_loop_cleanup;
    }
  }

  pk_loop_run_simple(loop);

sd_loop_cleanup:
  if (rep_socket != NULL) pk_endpoint_destroy(&rep_socket);

  if (cmd_info != NULL) free(cmd_info);

  sd_destroy(&sd_ctx);
  pk_loop_destroy(&loop);

  return ret;
}

<<<<<<< 6802ba4b3aa224f66a7220af7463e95b413c102e:package/libpiksi/libpiksi/src/settings.c
bool settings_loop_simple(const char *metrics_ident, register_settings_fn do_settings_register)
{
  return settings_loop(metrics_ident, NULL, NULL, NULL, do_settings_register, NULL, NULL, NULL);
}

int settings_loop_send_command(const char *metrics_ident,
                               const char *target_description,
                               const char *command,
                               const char *command_description,
                               const char *control_socket)
=======
bool sd_loop_simple(register_sd_fn do_sd_register)
{
  return sd_loop(NULL, NULL, NULL, do_sd_register, NULL, NULL, NULL);
}

int sd_loop_send_command(const char *target_description,
                         const char *command,
                         const char *command_description,
                         const char *control_socket)
>>>>>>> Settings naming rework:package/libpiksi/libpiksi/src/settings_daemon.c
{
#define CHECK_PK_EPT_ERR(COND, FUNC)         \
  if (COND) {                                \
    piksi_log(LOG_ERR,                       \
              "%s: error in %s (%s:%d): %s", \
              __FUNCTION__,                  \
              __STRING(FUNC),                \
              __FILE__,                      \
              __LINE__,                      \
              pk_endpoint_strerror());       \
    fprintf(stderr,                          \
            "%s: error in %s (%s:%d): %s",   \
            __FUNCTION__,                    \
            __STRING(FUNC),                  \
            __FILE__,                        \
            __LINE__,                        \
            pk_endpoint_strerror());         \
    return -1;                               \
  }

#define CMD_INFO_MSG "Sending '%s' command to %s..."

  piksi_log(LOG_INFO, CMD_INFO_MSG, command_description, target_description);
  printf(CMD_INFO_MSG "\n", command_description, target_description);

  pk_endpoint_t *req_socket =
    pk_endpoint_create(pk_endpoint_config()
                         .endpoint(control_socket)
                         .identity(get_socket_ident(metrics_ident, "control/req"))
                         .type(PK_ENDPOINT_REQ)
                         .get());
  CHECK_PK_EPT_ERR(req_socket == NULL, pk_endpoint_create);

  int ret = 0;
  u8 result = 0;

  ret = pk_endpoint_send(req_socket, command, strlen(command));
  CHECK_PK_EPT_ERR(ret < 0, pk_endpoint_send);

  ret = pk_endpoint_read(req_socket, &result, sizeof(result));
  CHECK_PK_EPT_ERR(ret < 0, pk_endpoint_read);

#define CMD_RESULT_MSG "Result of '%s' command: %hhu"

  piksi_log(LOG_INFO, CMD_RESULT_MSG, command_description, ret);
  printf(CMD_RESULT_MSG "\n", command_description, ret);

  pk_endpoint_destroy(&req_socket);

  return 0;

#undef CMD_RESULT_MSG
#undef CHECK_PK_EPT_ERR
#undef CMD_INFO_MSG
}
