/*
 * Copyright (C) 2017 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
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

#include "common.h"

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
 * @brief   Read and process incoming data.
 * @details Read and process a single incoming ZMQ message.
 *
 * @note    This function will block until a ZMQ message is received. For
 *          nonblocking operation, use the pollitem or reader APIs.
 *
 * @param[in] ctx           Pointer to the context to use.
 *
 * @return                  The operation result.
 * @retval 0                A message was successfully read and processed.
 * @retval -1               An error occurred.
 */
int settings_read(settings_ctx_t *ctx);

/**
 * @brief   Initialize a ZMQ pollitem.
 * @details Initialize a ZMQ pollitem to be used to poll the associated ZMQ
 *          socket for pending messages.
 *
 * @see     czmq, @c zmq_poll().
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[out] pollitem     Pointer to the ZMQ pollitem to initialize.
 *
 * @return                  The operation result.
 * @retval 0                The ZMQ pollitem was initialized successfully.
 * @retval -1               An error occurred.
 */
int settings_pollitem_init(settings_ctx_t *ctx, zmq_pollitem_t *pollitem);

/**
 * @brief   Check a ZMQ pollitem.
 * @details Check a ZMQ pollitem for pending messages and read a single
 *          incoming ZMQ message from the associated socket if available.
 *
 * @see     czmq, @c zmq_poll().
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] pollitem      Pointer to the ZMQ pollitem to check.
 *
 * @return                  The operation result.
 * @retval 0                The ZMQ pollitem was checked successfully.
 * @retval -1               An error occurred.
 */
int settings_pollitem_check(settings_ctx_t *ctx, zmq_pollitem_t *pollitem);

/**
 * @brief   Add a reader to a ZMQ loop.
 * @details Add a reader for the associated socket to a ZMQ loop. The reader
 *          will be executed to process pending messages when available.
 *
 * @note    Pending messages will only be processed while the ZMQ loop is
 *          running. See @c zloop_start().
 *
 * @see     czmq, @c zloop_start().
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] zloop         Pointer to the ZMQ loop to use.
 *
 * @return                  The operation result.
 * @retval 0                The reader was added successfully.
 * @retval -1               An error occurred.
 */
int settings_reader_add(settings_ctx_t *ctx, zloop_t *zloop);

/**
 * @brief   Remove a reader from a ZMQ loop.
 * @details Remove a reader for the associated socket from a ZMQ loop.
 *
 * @param[in] ctx           Pointer to the context to use.
 * @param[in] zloop         Pointer to the ZMQ loop to use.
 *
 * @return                  The operation result.
 * @retval 0                The reader was removed successfully.
 * @retval -1               An error occurred.
 */
int settings_reader_remove(settings_ctx_t *ctx, zloop_t *zloop);

#endif /* LIBPIKSI_SETTINGS_H */

/** @} */
