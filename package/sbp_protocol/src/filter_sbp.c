/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <limits.h>
#include <libsbp/sbp.h>
#include <syslog.h>

// clang-format off
#define SBP_MSG_TYPE_OFFSET 1
#define SBP_MSG_SIZE_MIN    6

#define RULES_COUNT_INIT  256
// clang-format on

typedef struct {
  uint16_t type;
  uint8_t divisor;
  uint8_t counter;
} filter_sbp_rule_t;

typedef struct {
  filter_sbp_rule_t *rules;
  uint32_t rules_count;
  const char *config_file;
  int config_inotify;
} filter_sbp_state_t;

static int process_rule(filter_sbp_rule_t *rule)
{
  /* Reject message if divisor is zero */
  if (rule->divisor == 0) {
    return 1;
  }

  /* Pass message when counter == divisor */
  if (++rule->counter >= rule->divisor) {
    rule->counter = 0;
    return 0;
  }

  /* Otherwise reject */
  return 1;
}

static void filter_sbp_load_config(filter_sbp_state_t *s)
{
  s->rules_count = 0;
  uint32_t rules_buffer_count = RULES_COUNT_INIT;

  /* Allocate buffer for rules */
  s->rules = malloc(rules_buffer_count * sizeof(filter_sbp_rule_t));
  if (s->rules == NULL) {
    syslog(LOG_ERR, "error allocating buffer for rules");
    rules_buffer_count = 0;
    return;
  }

  /* Open file */
  FILE *fp = fopen(s->config_file, "r");
  if (fp == NULL) {
    syslog(LOG_ERR, "error opening %s", s->config_file);
    return;
  }

  /* Read lines of file */
  bool error = false;
  char line[256];
  while (fgets(line, sizeof(line), fp) != NULL) {

    /* Expected format: <msg_type> <divisor> */
    unsigned int msg_type;
    unsigned int divisor;
    if (sscanf(line, "%x %x", &msg_type, &divisor) != 2) {
      syslog(LOG_ERR, "error parsing %s", s->config_file);
      error = true;
      break;
    }

    /* Reallocate rules buffer if required */
    if (s->rules_count >= rules_buffer_count) {
      rules_buffer_count *= 2;
      s->rules = realloc(s->rules, rules_buffer_count * sizeof(filter_sbp_rule_t));
      if (s->rules == NULL) {
        syslog(LOG_ERR, "error reallocating buffer for rules");
        rules_buffer_count = 0;
        error = true;
        break;
      }
    }

    /* Set rule */
    filter_sbp_rule_t *rule = &s->rules[s->rules_count++];
    rule->type = msg_type;
    rule->divisor = divisor;
    rule->counter = 0;
  }

  /* Close file */
  fclose(fp);

  /* Clear rules if an error occurred */
  if (error) {
    s->rules_count = 0;
  }
}

void *filter_create(const char *filename)
{
  filter_sbp_state_t *s = (filter_sbp_state_t *)malloc(sizeof(*s));
  if (s == NULL) {
    return NULL;
  }

  s->config_file = strdup(filename);
  filter_sbp_load_config(s);
  s->config_inotify = inotify_init1(IN_NONBLOCK);
  int wd = inotify_add_watch(s->config_inotify, filename, IN_CLOSE_WRITE);
  if ((s->config_inotify < 0) || (wd < 0)) {
    syslog(LOG_ERR, "error setting up inotify on config file: %s", filename);
  }

  return (void *)s;
}

void filter_destroy(void **state)
{
  filter_sbp_state_t *s = (filter_sbp_state_t *)(*state);
  if (s->rules != NULL) {
    free(s->rules);
    s->rules = NULL;
    s->rules_count = 0;
  }
  close(s->config_inotify);
  s->config_inotify = -1;
  free((void *)s->config_file);
  s->config_file = NULL;
  free(*state);
  *state = NULL;
}

int filter_process(void *state, const uint8_t *msg, uint32_t msg_length)
{
  filter_sbp_state_t *s = (filter_sbp_state_t *)state;

  /* Reload config if changed */
  char buf[sizeof(struct inotify_event) + NAME_MAX + 1]
    __attribute__((aligned(__alignof__(struct inotify_event))));
  if (read(s->config_inotify, buf, sizeof(buf)) > 0) {
    /* Any events on the config file trigger a reload.
     * We only subscribe to modify events. */
    if (s->rules) {
      free(s->rules);
      s->rules = NULL;
      s->rules_count = 0;
    }
    filter_sbp_load_config(s);
  }

  /* Pass everything if no rules are configured */
  if (s->rules_count == 0) {
    return 0;
  }

  /* Reject invalid short messages */
  if (msg_length < SBP_MSG_SIZE_MIN) {
    return 1;
  }

  /* Search for corresponding rule */
  uint16_t msg_type = le16toh(*(uint16_t *)&msg[SBP_MSG_TYPE_OFFSET]);
  uint32_t i;
  for (i = 0; i < s->rules_count; i++) {
    filter_sbp_rule_t *rule = &s->rules[i];
    if (rule->type == msg_type) {
      return process_rule(rule);
    }
  }

  /* Reject message if no matching rule was found */
  return 1;
}
