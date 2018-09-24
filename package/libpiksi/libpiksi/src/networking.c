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

#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <linux/if_link.h>

#include <libpiksi/util.h>
#include <libpiksi/interface.h>
#include <libpiksi/networking.h>
#include <libpiksi/logging.h>

typedef s32 (*interface_map_fn_t)(struct ifaddrs *ifa, void *userdata);

// refactored from original implementation in piksi_system_daemon by @axlan
static void map_network_interfaces(interface_map_fn_t map_fn, void *userdata)
{
  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs(&ifaddr) == -1) {
    return;
  }

  /* Walk through linked list, maintaining head pointer so we
     can free list later */

  ifa = ifaddr;
  while (ifa != NULL) {
    if ((map_fn)(ifa, userdata) != 0) {
      break;
    }
    ifa = ifa->ifa_next;
  }

  freeifaddrs(ifaddr);
}

static int count_set_bits(const u8 *data, size_t num_bytes)
{
  int count = 0;
  for (int i = 0; i < num_bytes; i++) {
    count += __builtin_popcount(data[i]);
  }
  return count;
}

struct fill_network_state_s {
  msg_network_state_resp_t *interfaces;
  u8 interfaces_size;
  u8 *interfaces_count;
};

static s32 fill_network_state_struct(struct ifaddrs *ifa, void *userdata)
{
  struct fill_network_state_s *fill_state = (struct fill_network_state_s *)userdata;
  msg_network_state_resp_t *interfaces = fill_state->interfaces;
  const u8 interfaces_size = fill_state->interfaces_size;
  u8 *interfaces_count = fill_state->interfaces_count;

  if (interfaces == NULL || interfaces_count == NULL) {
    return -1;
  }

  // guard conditions to skip
  if (ifa->ifa_addr == NULL) {
    return 0;
  }
  if (ifa->ifa_name == NULL) {
    return 0;
  }
  if (strlen(ifa->ifa_name) >= sizeof(interfaces[0].interface_name)) {
    piksi_log(LOG_WARNING, "Network State: skipping ifa_name that is too long - %s", ifa->ifa_name);
    return 0;
  }

  // check if this is the first entry with this interface name
  msg_network_state_resp_t *cur_iface = NULL;
  for (int i = 0; i < interfaces_size; i++) {
    // reached the end of the list so add a new entry
    if (strlen(interfaces[i].interface_name) == 0) {
      cur_iface = &interfaces[i];
      strcpy(cur_iface->interface_name, ifa->ifa_name);
      (*interfaces_count)++;
      break;
    }
    // found the entry in the list so add new info to that entry
    if (strcmp(interfaces[i].interface_name, ifa->ifa_name) == 0) {
      cur_iface = &interfaces[i];
      break;
    }
  }
  if (cur_iface == NULL) {
    return 0;
  }

  switch (ifa->ifa_addr->sa_family) {
  case AF_INET: {
    const size_t IP4_SIZE = sizeof(cur_iface->ipv4_address);
    memcpy(cur_iface->ipv4_address,
           &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr,
           IP4_SIZE);
    u8 *mask_bytes = (u8 *)&((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr.s_addr;
    cur_iface->ipv4_mask_size = count_set_bits(mask_bytes, IP4_SIZE);
    cur_iface->flags = ifa->ifa_flags;
  } break;
  case AF_INET6: {
    const size_t IP6_SIZE = sizeof(cur_iface->ipv6_address);
    memcpy(cur_iface->ipv6_address,
           &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr.s6_addr,
           IP6_SIZE);
    u8 *mask_bytes = (u8 *)&((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr.s6_addr;
    cur_iface->ipv6_mask_size = count_set_bits(mask_bytes, IP6_SIZE);
  } break;
  case AF_PACKET: {
    if (ifa->ifa_data != NULL) {
      struct rtnl_link_stats *stats = ifa->ifa_data;
      cur_iface->rx_bytes = stats->rx_bytes;
      cur_iface->tx_bytes = stats->tx_bytes;
    }
  } break;
  default: {
    // nothing to do here
  } break;
  }

  return 0;
}

void query_network_state(msg_network_state_resp_t *interfaces,
                         u8 interfaces_n,
                         u8 *returned_interfaces)
{
  struct fill_network_state_s fill_data = {
    .interfaces = interfaces,
    .interfaces_size = interfaces_n,
    .interfaces_count = returned_interfaces,
  };
  *returned_interfaces = 0;
  map_network_interfaces(fill_network_state_struct, (void *)&fill_data);
}

struct fill_usage_struct_s {
  network_usage_t *usage_entries;
  u8 usage_entries_size;
  u8 *usage_entries_count;
  u64 duration;
};

static s32 fill_network_usage_struct(struct ifaddrs *ifa, void *userdata)
{
  struct fill_usage_struct_s *fill_usage = (struct fill_usage_struct_s *)userdata;
  network_usage_t *usage_entries = fill_usage->usage_entries;
  const u8 usage_entries_size = fill_usage->usage_entries_size;
  u8 *usage_entries_count = fill_usage->usage_entries_count;
  u64 duration = fill_usage->duration;

  if (usage_entries == NULL) {
    return -1;
  }
  if (usage_entries_count == NULL) {
    return -1;
  }
  if (*usage_entries_count >= usage_entries_size) {
    return -1;
  }

  // guard conditions to skip
  if (ifa->ifa_addr == NULL) {
    return 0;
  }
  if (ifa->ifa_name == NULL) {
    return 0;
  }
  if (strlen(ifa->ifa_name) >= sizeof(usage_entries[0].interface_name)) {
    piksi_log(LOG_WARNING, "Network Usage: skipping ifa_name that is too long - %s", ifa->ifa_name);
    return 0;
  }

  if (ifa->ifa_addr->sa_family == AF_PACKET && ifa->ifa_data != NULL) {
    network_usage_t *cur_iface = &usage_entries[*usage_entries_count];
    struct rtnl_link_stats *stats = ifa->ifa_data;
    strcpy(cur_iface->interface_name, ifa->ifa_name);
    cur_iface->duration = duration;
    cur_iface->rx_bytes = stats->rx_bytes;
    cur_iface->tx_bytes = stats->tx_bytes;
    cur_iface->total_bytes = cur_iface->rx_bytes + cur_iface->tx_bytes;
    (*usage_entries_count)++;
  }

  return 0;
}

void query_network_usage(network_usage_t *usage_entries, u8 usage_entries_n, u8 *interface_count)
{
  u64 uptime_ms = system_uptime_ms_get();
  const interface_t *ifa = NULL;
  interface_list_t *ifa_list = NULL;

  ifa_list = interface_list_create();
  if (ifa_list == NULL) {
    return;
  }
  if (interface_list_read_interfaces(ifa_list) != 0) {
    piksi_log(LOG_WARNING,
              "Error occured during interface read, some or all interfaces may not have been read");
  }

  for (ifa = interface_list_head(ifa_list); ifa; ifa = interface_next(ifa)) {
    const char *ifa_name = interface_name(ifa);
    const struct user_net_device_stats *stats = interface_stats(ifa);
    network_usage_t *cur_iface = &usage_entries[*interface_count];
    strcpy(cur_iface->interface_name, ifa_name);
    cur_iface->duration = uptime_ms;
    cur_iface->rx_bytes = stats->rx_bytes;
    cur_iface->tx_bytes = stats->tx_bytes;
    cur_iface->total_bytes = cur_iface->rx_bytes + cur_iface->tx_bytes;
    if ((*interface_count)++ >= usage_entries_n) {
      break;
    }
  }
  interface_list_destroy(&ifa_list);
}
