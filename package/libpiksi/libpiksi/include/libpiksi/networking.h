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
 * @file    networking.h
 * @brief   Networking Interface APIs
 *
 * @defgroup    networking Networking
 * @addtogroup  networking
 * @{
 */

#ifndef LIBPIKSI_NETWORKING_H
#define LIBPIKSI_NETWORKING_H

#include <libpiksi/common.h>
#include <libsbp/piksi.h>

/**
 * @brief Query system for network interfaces information
 * @param interfaces: container for interface information
 * @param interfaces_n: size of container
 * @param returned_interfaces_n: number of slots filled with interface information
 */
void query_network_state(msg_network_state_resp_t *interfaces, u8 interfaces_n, u8 *returned_interfaces);

/**
 * @brief Query system for network usage
 * @param usage_entries: container for usage information
 * @param usage_entries_n: size of container
 * @param interface_count: number of slots filled with usage information
 */
void query_network_usage(network_usage_t *usage_entries, u8 usage_entries_n, u8 *interface_count);

/**
 * @brief Query Usage on a Specific Interface
 * @param usage_entry: a usage struct to fill with results
 * @param interface_name: the name of the interface to filter on
 * @param found: status of the query, false if a match was not found
 */
void query_network_usage_by_interface(network_usage_t *usage_entry, u8* interface_name, bool *found);

#endif /* LIBPIKSI_NETWORKING_H */

/** @} */
