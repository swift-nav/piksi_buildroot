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
 * @file    settings.h
 * @brief   Settings API.
 *
 * @defgroup    settings Settings
 * @addtogroup  settings
 * @{
 */

#ifndef LIBPIKSI_SETTINGS_H
#define LIBPIKSI_SETTINGS_H

#include <libpiksi/common.h>
#include <libpiksi/sbp_rx.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Settings type.
 */
typedef int settings_type_t;

/**
 * @brief   Standard settings type definitions.
 */
enum {
  SETTINGS_TYPE_INT,      /**< Integer. 8, 16, or 32 bits.                   */
  SETTINGS_TYPE_FLOAT,    /**< Float. Single or double precision.            */
  SETTINGS_TYPE_STRING,   /**< String.                                       */
  SETTINGS_TYPE_BOOL      /**< Boolean.                                      */
};

/**
 * @brief   Settings notify callback.
 * @details Signature of a user-provided callback function to be executed
 *          after a setting value is updated.
 *
 * @note    The setting value will be updated _before_ the callback is executed.
 *          If the callback returns an error, the setting value will be
 *          reverted to the previous value.
 *
 * @param[in] context       Pointer to the user-provided context.
 *
 * @return                  The operation result.
 * @retval 0                Success. The updated setting value is acceptable.
 * @retval -1               The updated setting value should be reverted.
 */
typedef int (*settings_notify_fn)(void *context);

/**
 * @struct  settings_ctx_t
 *
 * @brief   Opaque context for settings.
 */
typedef struct settings_ctx_s settings_ctx_t;

/**
 * @brief Parse a setting message to obtain the section, name and value
 *
 * @param[in] msg          raw sbp message
 * @param[in] msg_n        length of sbp message
 *
 * @param[out] section     reference to location of section string in message
 * @param[out] name        reference to location of name string in message
 * @param[out] value       reference to location of value string in message
 *
 * @return                 The operation result.
 * @retval 0               Parsing operation successful
 * @retval -1              An error occurred.
 */
int setting_parse_setting_text(const u8 *msg,
                               u8 msg_n,
                               const char **section,
                               const char **name,
                               const char **value);

/**
 * @brief   Create a settings context.
 * @details Create and initialize a settings context.
 *
 * @return                  Pointer to the created context, or NULL if the
 *                          operation failed.
 */
settings_ctx_t * settings_create(void);

/**
 * @brief   Destroy a settings context.
 * @details Deinitialize and destroy a settings context.
 *
 * @note    The context pointer will be set to NULL by this function.
 *
 * @param[inout] ctx        Double pointer to the context to destroy.
 */
void settings_destroy(settings_ctx_t **ctx);

/**
 * @brief   Register an enum type.
 * @details Register an enum as a settings type.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] enum_names    Null-terminated array of strings corresponding to
 *                          the possible enum values.
 * @param[out] type         Pointer to be set to the allocated settings type.
 *
 * @return                  The operation result.
 * @retval 0                The enum type was registered successfully.
 * @retval -1               An error occurred.
 */
int settings_type_register_enum(settings_ctx_t *ctx,
                                const char * const enum_names[],
                                settings_type_t *type);

/**
 * @brief   Register a setting.
 * @details Register a persistent, user-facing setting.
 *
 * @note    The specified notify function will be executed from this function
 *          during initial registration.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] section       String describing the setting section.
 * @param[in] name          String describing the setting name.
 * @param[in] var           Address of the setting variable. This location will
 *                          be written directly by the settings module.
 * @param[in] var_len       Size of the setting variable.
 * @param[in] type          Type of the setting.
 * @param[in] notify        Notify function to be executed when the setting is
 *                          written and during initial registration.
 * @param[in] notify_context Context passed to the notify function.
 *
 * @return                  The operation result.
 * @retval 0                The setting was registered successfully.
 * @retval -1               An error occurred.
 */
int settings_register(settings_ctx_t *ctx, const char *section,
                      const char *name, void *var, size_t var_len,
                      settings_type_t type, settings_notify_fn notify,
                      void *notify_context);

/**
 * @brief   Register a read-only setting.
 * @details Register a read-only, user-facing setting.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] section       String describing the setting section.
 * @param[in] name          String describing the setting name.
 * @param[in] var           Address of the setting variable. This location will
 *                          be written directly by the settings module.
 * @param[in] var_len       Size of the setting variable.
 * @param[in] type          Type of the setting.
 *
 * @return                  The operation result.
 * @retval 0                The setting was registered successfully.
 * @retval -1               An error occurred.
 */
int settings_register_readonly(settings_ctx_t *ctx, const char *section,
                               const char *name, const void *var,
                               size_t var_len, settings_type_t type);

/**
 * @brief   Create and add a watch only setting.
 * @details Create and add a watch only setting.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] section       String describing the setting section.
 * @param[in] name          String describing the setting name.
 * @param[in] var           Address of the setting variable. This location will
 *                          be written directly by the settings module.
 * @param[in] var_len       Size of the setting variable.
 * @param[in] type          Type of the setting.
 * @param[in] notify        Notify function to be executed when the setting is
 *                          updated by a write response
 * @param[in] notify_context Context passed to the notify function.
 *
 * @return                  The operation result.
 * @retval 0                The setting was registered successfully.
 * @retval -1               An error occurred.
 */
int settings_add_watch(settings_ctx_t *ctx, const char *section,
                       const char *name, void *var, size_t var_len,
                       settings_type_t type, settings_notify_fn notify,
                       void *notify_context);

/**
 * @brief   Attach settings context to Piksi loop.
 * @details Attach settings context to Piksi loop. Settings rx callbacks
 *          will be executed to process pending messages when available.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] pk_loop       Pointer to the Piksi loop to use.
 *
 * @return                  The operation result.
 * @retval 0                The settings reader was attached successfully.
 * @retval -1               An error occurred.
 */
int settings_attach(settings_ctx_t *ctx, pk_loop_t *pk_loop);

/**
 * @brief   Registers settings with the given context.
 *
 * @details Intended to be used with @c settings_loop to register the settings
 *          that the loop will be responsible for.
 */
typedef void (*register_settings_fn)(settings_ctx_t *ctx);

/**
 * @brief   Handles a control message for a @c settings_loop
 *
 * @details Intended to be used with @c settings_loop, this function is called
 *          when the control command for the loop is received.
 */
typedef bool (*handle_command_fn)();

/**
 * @brief   Start a settings loop with a control sock.
 * @details Starts a settings loop with a control socket, this is the main
 *          entry point for a daemon that handles settings and has a simple
 *          (one command) control socket.
 *
 * @param[in] control_socket       The control socket URL (in ZMQ format),
 *                                 such as ipc://path/socket.unix
 * @param[in] control_socket_file  The path of the control socket on the file
 *                                 system, usually a substring of
 *                                 @c control_socket
 * @param[in] control_command      Single character string that will trigger
 *                                 @c do_handle_command when written to the
 *                                 @c control_socket.
 * @param[in] do_register_settings Function that will register the settings
 *                                 for this loop
 * @param[in] do_handle_command    Function that handles the control command
 *
 * @return                  Settings loop exit status
 * @retval 0                Successful exit
 * @retval -1               An error occurred
 */
bool settings_loop(const char* control_socket,
                   const char* control_socket_file,
                   const char* control_command,
                   register_settings_fn do_register_settings,
                   handle_command_fn do_handle_command);

bool settings_loop_simple(register_settings_fn do_register_settings);

/**
 * @brief   Send a control command to a running settings daemon
 * @details Sends a control command to a daemon running with
 *          a control socket setup by @c settings_loop
 *
 * @param[in] target_description   Description of the target for logging
 * @param[in] command              The command to send
 * @param[in] command_description  Description of the command for logging
 * @param[in] control_socket       The ZMQ URL of the control socket to write
 *                                 the command to.
 *
 * @return                  Result of the command, value depends on
 *                          the command invoked.
 */
int settings_loop_send_command(const char* target_description,
                               const char* command,
                               const char* command_description,
                               const char* control_socket);

#ifdef __cplusplus
}
#endif

#endif /* LIBPIKSI_SETTINGS_H */

/** @} */
