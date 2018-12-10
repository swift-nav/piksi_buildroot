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

#include <limits.h>
#include <stdint.h>

#include <libpiksi/cast_check.h>
#include <libpiksi/common.h>
#include <libpiksi/interface.h>
#include <libpiksi/logging.h>

#define PROC_NET_DEV "/proc/net/dev"
#define PROC_NET_DEV_MAX_LINE_SIZE (512u)

#ifndef IF_NAMESIZE
#define IF_NAMESIZE (16)
#endif

struct interface_s {
  char name[IF_NAMESIZE];
  struct interface_s *next;
  struct interface_s *prev;
  struct user_net_device_stats stats;
};

struct interface_list_s {
  interface_t *head;
  interface_t *tail;
};

const interface_t *interface_next(const interface_t *ifa)
{
  return ifa->next;
}

const interface_t *interface_prev(const interface_t *ifa)
{
  return ifa->prev;
}

const char *interface_name(const interface_t *ifa)
{
  return ifa->name;
}

const struct user_net_device_stats *interface_stats(const interface_t *ifa)
{
  return &ifa->stats;
}

static void interface_list_init(interface_list_t *ifa_list)
{
  ifa_list->head = NULL;
  ifa_list->tail = NULL;
}

interface_list_t *interface_list_create(void)
{
  interface_list_t *ifa_list = NULL;
  ifa_list = (interface_list_t *)malloc(sizeof(interface_list_t));
  if (ifa_list != NULL) {
    interface_list_init(ifa_list);
  }
  return ifa_list;
}

void interface_list_destroy(interface_list_t **ifa_list_loc)
{
  interface_list_t *ifa_list = *ifa_list_loc;
  if (ifa_list_loc == NULL || ifa_list == NULL) {
    return;
  }
  interface_t *ifa = ifa_list->tail;
  while (ifa) {
    interface_t *ifa_to_free = ifa;
    ifa = ifa->prev;
    free(ifa_to_free);
  }
  free(ifa_list);
  *ifa_list_loc = NULL;
}

interface_t *interface_list_head(interface_list_t *ifa_list)
{
  return ifa_list->head;
}

interface_t *interface_list_tail(interface_list_t *ifa_list)
{
  return ifa_list->tail;
}

static interface_t *interface_list_add(interface_list_t *ifa_list, char *ifa_name)
{
  interface_t *ifa;
  interface_t *new;

  // check if name exists in list already
  // or place in order based on comparison
  for (ifa = ifa_list->tail; ifa != NULL; ifa = ifa->prev) {
    int cmp = strcmp(ifa->name, ifa_name);
    // found existing
    if (cmp == 0) {
      return ifa;
    }
    // found neighbor
    if (cmp < 0) {
      break;
    }
  }

  new = (interface_t *)malloc(sizeof(interface_t));
  if (new == NULL) {
    return new;
  }
  memset(new, 0, sizeof(interface_t));
  strncpy(new->name, ifa_name, IF_NAMESIZE);

  // We are adding the head
  if (ifa == NULL) {
    if (ifa_list->head == NULL) {
      ifa_list->tail = new; // was empty list
    } else {                // old head linked to new
      new->next = ifa_list->head;
      ifa_list->head->prev = new;
    }
    ifa_list->head = new;
  } else { // linking to existing
    new->prev = ifa;
    if (ifa->next == NULL) {
      ifa_list->tail = new; // existing is tail
    } else {                // existing has next
      new->next = ifa->next;
      ifa->next->prev = new;
    }
    ifa->next = new;
  }
  return new;
}

/* start of busybox util section */
/*
 * stolen from net-tools-1.59 and stripped down for busybox by
 *            Erik Andersen <andersen@codepoet.org>
 *
 * Heavily modified by Manuel Novoa III       Mar 12, 2001
 *
 * Added print_bytes_scaled function to reduce code size.
 * Added some (potentially) missing defines.
 * Improved display support for -a and for a named interface.
 *
 * -----------------------------------------------------------
 *
 * ifconfig   This file contains an implementation of the command
 *              that either displays or sets the characteristics of
 *              one or more of the system's networking interfaces.
 *
 *
 * Author:      Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *              and others.  Copyright 1993 MicroWalt Corporation
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 *
 * Patched to support 'add' and 'del' keywords for INET(4) addresses
 * by Mrs. Brisby <mrs.brisby@nimh.org>
 *
 * {1.34} - 19980630 - Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *                     - gettext instead of catgets for i18n
 *          10/1998  - Andi Kleen. Use interface list primitives.
 *          20001008 - Bernd Eckenfels, Patch from RH for setting mtu
 *            (default AF was wrong)
 */
static char *skip_whitespace(const char *s)
{
  /* In POSIX/C locale (the only locale we care about: do we REALLY want
   * to allow Unicode whitespace in, say, .conf files? nuts!)
   * isspace is only these chars: "\t\n\v\f\r" and space.
   * "\t\n\v\f\r" happen to have ASCII codes 9,10,11,12,13.
   * Use that.
   */
  while (*s == ' ' || (unsigned char)(*s - 9) <= (13 - 9))
    s++;

  return (char *)s;
}

static char *get_name(char name[IF_NAMESIZE], char *p)
{
  /* Extract NAME from nul-terminated p of the form "<whitespace>NAME:"
   * If match is not made, set NAME to "" and return unchanged p.
   */
  char *nameend;
  char *namestart;

  nameend = namestart = skip_whitespace(p);

  for (;;) {
    if ((nameend - namestart) >= IF_NAMESIZE) break; /* interface name too large - return "" */
    if (*nameend == ':') {
      size_t name_size = long_to_sizet(nameend - namestart);
      memcpy(name, namestart, name_size);
      name[name_size] = '\0';
      return nameend + 1;
    }
    nameend++;
    /* isspace, NUL, any control char? */
    if ((unsigned char)*nameend <= (unsigned char)' ')
      break; /* trailing ':' not found - return "" */
  }
  name[0] = '\0';
  return p;
}

/* If scanf supports size qualifiers for %n conversions, then we can
 * use a modified fmt that simply stores the position in the fields
 * having no associated fields in the proc string.  Of course, we need
 * to zero them again when we're done.  But that is smaller than the
 * old approach of multiple scanf occurrences with large numbers of
 * args. */

/* static const char *const ss_fmt[] = { */
/*    "%lln%llu%lu%lu%lu%lu%ln%ln%lln%llu%lu%lu%lu%lu%lu", */
/*    "%llu%llu%lu%lu%lu%lu%ln%ln%llu%llu%lu%lu%lu%lu%lu", */
/*    "%llu%llu%lu%lu%lu%lu%lu%lu%llu%llu%lu%lu%lu%lu%lu%lu" */
/* }; */

/* We use %n for unavailable data in older versions of /proc/net/dev formats.
 * This results in bogus stores to ife->FOO members corresponding to
 * %n specifiers (even the size of integers may not match).
 */
#if INT_MAX == LONG_MAX
static const char *const ss_fmt[] = {
  "%n%llu%u%u%u%u%n%n%n%llu%u%u%u%u%u",
  "%llu%llu%u%u%u%u%n%n%llu%llu%u%u%u%u%u",
  "%llu%llu%u%u%u%u%u%u%llu%llu%u%u%u%u%u%u",
};
#else
static const char *const ss_fmt[] = {
  "%n%llu%lu%lu%lu%lu%n%n%n%llu%lu%lu%lu%lu%lu",
  "%llu%llu%lu%lu%lu%lu%n%n%llu%llu%lu%lu%lu%lu%lu",
  "%llu%llu%lu%lu%lu%lu%lu%lu%llu%llu%lu%lu%lu%lu%lu%lu",
};
#endif

static void get_dev_fields(char *bp, interface_t *ife, int procnetdev_vsn)
{
  memset(&ife->stats, 0, sizeof(struct user_net_device_stats));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"

  sscanf(bp,
         ss_fmt[procnetdev_vsn],
         &ife->stats.rx_bytes,
         &ife->stats.rx_packets,
         &ife->stats.rx_errors,
         &ife->stats.rx_dropped,
         &ife->stats.rx_fifo_errors,
         &ife->stats.rx_frame_errors,
         &ife->stats.rx_compressed,
         &ife->stats.rx_multicast,
         &ife->stats.tx_bytes,
         &ife->stats.tx_packets,
         &ife->stats.tx_errors,
         &ife->stats.tx_dropped,
         &ife->stats.tx_fifo_errors,
         &ife->stats.collisions,
         &ife->stats.tx_carrier_errors,
         &ife->stats.tx_compressed);

#pragma GCC diagnostic pop

  if (procnetdev_vsn <= 1) {
    if (procnetdev_vsn == 0) {
      ife->stats.rx_bytes = 0;
      ife->stats.tx_bytes = 0;
    }
    ife->stats.rx_multicast = 0;
    ife->stats.rx_compressed = 0;
    ife->stats.tx_compressed = 0;
  }
}

static int procnetdev_version(char *buf)
{
  if (strstr(buf, "compressed")) return 2;
  if (strstr(buf, "bytes")) return 1;
  return 0;
}
/* end of busybox util section */

/**
 * @brief interface_list_read_interfaces
 * Implements busybox utils to populate interface list with
 * statistics for the interface
 * @param ifa_list: interface list to populate
 * @return 0 on nominal operation, -1 if an error occured
 */
int interface_list_read_interfaces(interface_list_t *ifa_list)
{
  int result = -1;
  FILE *proc_net_dev;
  char line[PROC_NET_DEV_MAX_LINE_SIZE];
  interface_t *ifa = NULL;
  int procnetdev_vsn;

  // Open the proc file descriptor
  proc_net_dev = fopen(PROC_NET_DEV, "r");
  if (proc_net_dev == NULL) {
    return result;
  }

  // We toss the first line
  fgets(line, sizeof(line), proc_net_dev);
  fgets(line, sizeof(line), proc_net_dev);

  // Gets the proper format string version to pull stats
  procnetdev_vsn = procnetdev_version(line);

  while (fgets(line, sizeof(line), proc_net_dev) != 0) {
    char *parse_pos = line;
    char ifa_name[IF_NAMESIZE];
    parse_pos = get_name(ifa_name, parse_pos);
    if (parse_pos == line || ifa_name[0] == '\0') {
      piksi_log(LOG_ERR, "Error parsing interface name from proc");
      goto cleanup;
    }
    ifa = interface_list_add(ifa_list, ifa_name);
    if (ifa == NULL) {
      piksi_log(LOG_ERR, "Error allocating interface member");
      goto cleanup;
    }
    get_dev_fields(parse_pos, ifa, procnetdev_vsn);
  }

  result = 0; // Success

cleanup:
  fclose(proc_net_dev);
  return result;
}
