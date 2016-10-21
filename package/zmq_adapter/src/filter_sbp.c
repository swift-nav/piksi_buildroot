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

#include "filter_sbp.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SBP_MSG_TYPE_OFFSET 1
#define SBP_MSG_SIZE_MIN    6

#define RULES_COUNT_INIT  256

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

void filter_sbp_init(void *filter_sbp_state, const char *filename)
{
  filter_sbp_state_t *s = (filter_sbp_state_t *)filter_sbp_state;
  s->rules_count = 0;
  uint32_t rules_buffer_count = RULES_COUNT_INIT;

  /* Allocate buffer for rules */
  s->rules = malloc(rules_buffer_count * sizeof(filter_sbp_rule_t));
  if (s->rules  == NULL) {
    printf("error allocating buffer for rules\n");
    rules_buffer_count = 0;
    return;
  }

  /* Open file */
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("error opening %s\n", filename);
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
      printf("error parsing %s\n", filename);
      error = true;
      break;
    }

    /* Reallocate rules buffer if required */
    if (s->rules_count >= rules_buffer_count) {
      rules_buffer_count *= 2;
      s->rules = realloc(s->rules,
                         rules_buffer_count * sizeof(filter_sbp_rule_t));
      if (s->rules  == NULL) {
        printf("error reallocating buffer for rules\n");
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

int filter_sbp_process(void *filter_sbp_state,
                       const uint8_t *msg, uint32_t msg_length)
{
  filter_sbp_state_t *s = (filter_sbp_state_t *)filter_sbp_state;

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
