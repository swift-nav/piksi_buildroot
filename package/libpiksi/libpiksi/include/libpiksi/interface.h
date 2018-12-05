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


#ifndef LIBPIKSI_INTERFACE_H
#define LIBPIKSI_INTERFACE_H

/* for busybox util section */
struct user_net_device_stats {
  unsigned long long rx_packets; /* total packets received       */
  unsigned long long tx_packets; /* total packets transmitted    */
  unsigned long long rx_bytes;   /* total bytes received         */
  unsigned long long tx_bytes;   /* total bytes transmitted      */
  unsigned long rx_errors;       /* bad packets received         */
  unsigned long tx_errors;       /* packet transmit problems     */
  unsigned long rx_dropped;      /* no space in linux buffers    */
  unsigned long tx_dropped;      /* no space available in linux  */
  unsigned long rx_multicast;    /* multicast packets received   */
  unsigned long rx_compressed;
  unsigned long tx_compressed;
  unsigned long collisions;

  /* detailed rx_errors: */
  unsigned long rx_length_errors;
  unsigned long rx_over_errors;   /* receiver ring buff overflow  */
  unsigned long rx_crc_errors;    /* recved pkt with crc error    */
  unsigned long rx_frame_errors;  /* recv'd frame alignment error */
  unsigned long rx_fifo_errors;   /* recv'r fifo overrun          */
  unsigned long rx_missed_errors; /* receiver missed packet     */
  /* detailed tx_errors */
  unsigned long tx_aborted_errors;
  unsigned long tx_carrier_errors;
  unsigned long tx_fifo_errors;
  unsigned long tx_heartbeat_errors;
  unsigned long tx_window_errors;
};

typedef struct interface_s interface_t;
typedef struct interface_list_s interface_list_t;

const interface_t *interface_next(const interface_t *ifa);
const interface_t *interface_prev(const interface_t *ifa);
const char *interface_name(const interface_t *ifa);
const struct user_net_device_stats *interface_stats(const interface_t *ifa);

interface_list_t *interface_list_create(void);
void interface_list_destroy(interface_list_t **ifa_list_loc);
interface_t *interface_list_head(interface_list_t *ifa_list);
interface_t *interface_list_tail(interface_list_t *ifa_list);
int interface_list_read_interfaces(interface_list_t *ifa_list);

#endif /* LIBPIKSI_INTERFACE_H */
