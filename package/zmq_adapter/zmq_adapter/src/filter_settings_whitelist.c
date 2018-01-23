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

#include "filter_settings_whitelist.h"

#include <cmph.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>

#include <libpiksi/logging.h>

#include <libsbp/sbp.h>
#include <libsbp/settings.h>

#define SBP_MSG_TYPE_OFFSET 1
#define SBP_MSG_SIZE_MIN    6

extern bool debug;

#define debug_printf(format, ...) \
  if (debug) { \
    fprintf(stdout, "[PID %d] %s+%d(%s) " format, getpid(), \
      __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
    fflush(stdout); }

#define MSG_ERROR_EMPTY_LINES \
  "filter_settings_whitelist: invalid config file, lines cannot be empty"

#define MSG_ERROR_NO_TRAILING_SPACES \
  "filter_settings_whitelist: invalid config file, leading spaces not allowed"

#define MSG_ERROR_NO_LEADING_SPACES \
  "filter_settings_whitelist: invalid config file, trailing spaces not allowed"

typedef struct {
  const uint8_t *msg;
  uint32_t msg_length;
} read_context_t;

typedef struct {
  size_t offset;
  size_t length;
} whitelist_entry_t;

typedef struct {
  cmph_t *hash;
  cmph_io_adapter_t *source;
  int whitelist_fd;
  const char* whitelist;
  size_t whitelist_size;
  FILE* keys_fp;
  whitelist_entry_t* whitelist_entries;
  size_t whitelist_entry_count;
  bool reject;
} filter_swl_state_t;

/* Parse SBP message payload into setting parameters */
static bool parse_setting(u8 len, u8 msg[],
                          const char **section,
                          const char **setting,
                          const char **value)
{
  *section = NULL;
  *setting = NULL;
  *value = NULL;

  if (len == 0) {
    return false;
  }

  if (msg[len-1] != '\0') {
    return false;
  }

  if ((len == 0) || (msg[len-1] != '\0')) {
    return false;
  }

  /* Extract parameters from message:
   * 3 null terminated strings: section, setting and value
   */
  *section = (const char *)msg;
  for (int i = 0, tok = 0; i < len; i++) {
    if (msg[i] == '\0') {
      tok++;
      switch (tok) {
      case 1:
        *setting = (const char *)&msg[i+1];
        break;
      case 2:
        if (i + 1 < len)
          *value = (const char *)&msg[i+1];
        break;
      case 3:
        if (i == len-1)
          break;
      default:
        break;
      }
    }
  }

  return true;
}

static bool setting_in_whitelist(filter_swl_state_t* state, char* setting_name, size_t length)
{
  if (setting_name == NULL) {
    piksi_log(LOG_WARNING, "settings name is NULL");
    return false;
  }

  if (length == 0) {
    piksi_log(LOG_WARNING, "settings name has zero length");
    return false;
  }

  uint32_t index = cmph_search(state->hash, setting_name, length);

  if (index >= state->whitelist_entry_count) {
    return false;
  }

  if (length != state->whitelist_entries[index].length) {
    return false;
  }

  const char* whitelist_entry =
    state->whitelist + state->whitelist_entries[index].offset;

  if (strncmp(whitelist_entry, setting_name, length) != 0)
    return false;

  return true;
}

static void write_callback(u16 sender_id, u8 len, u8 msg[], void* context)
{
  (void)sender_id;

  filter_swl_state_t* state = (filter_swl_state_t*) context;
  state->reject = false;

  const char *section = NULL;
  const char *setting = NULL;
  const char *value = NULL;

  if (!parse_setting(len, msg, &section, &setting, &value)) {
    debug_printf("filter: write_callback: failed to parse setting\n");
    return;
  }

  debug_printf("filter: sensitive write check: section: %s, setting: %s, value: %s\n", section, setting, value);

  char setting_composed[512] = { 0 };
  int setting_composed_len =
    snprintf(setting_composed, sizeof(setting_composed), "%s.%s", section, setting);

  if (setting_composed_len >= sizeof(setting_composed))
    return;

  if (setting_in_whitelist(state, setting_composed, setting_composed_len))
    return;

  const char* reject_msg = "This interface is not allowed to write sensitive settings";

  syslog(LOG_ERR, reject_msg);
  sbp_log(LOG_ERR, reject_msg);

  state->reject = true;
}

static u32 one_buffer_read(u8* buff, u32 n, void* context)
{
  read_context_t* ctx = (read_context_t*) context;
  size_t read_size = (size_t)(n <= ctx->msg_length ? n : ctx->msg_length);

  memcpy(buff, ctx->msg, read_size);

  ctx->msg += read_size;
  ctx->msg_length -= read_size;

  return read_size;
}

static bool reject_sensitive_settings_write(filter_swl_state_t* state, const uint8_t *msg, uint32_t msg_length)
{
  // Not an SBP message, reject?
  if (msg_length < SBP_MSG_SIZE_MIN) {
    syslog(LOG_ERR, "filter: rejecting short SBP message");
    return true;
  }

  /* Search for corresponding rule */
  uint16_t msg_type = le16toh(*(uint16_t *)&msg[SBP_MSG_TYPE_OFFSET]);
 
  if (msg_type != SBP_MSG_SETTINGS_WRITE) {
    debug_printf("filter: msg_type: %04hx\n", msg_type);
    debug_printf("filter: msg_type: %02hhx, %02hhx, %02hhx, %02hhx, %02hhx, %02hhx\n",
        msg[0], msg[1], msg[2], msg[3], msg[4], msg[5]);
    return false;
  }

  debug_printf("filter: checking settings write message\n");

  sbp_state_t sbp_state;
  sbp_state_init(&sbp_state);

  read_context_t context = {
    .msg        = msg,
    .msg_length = msg_length,
  };

  sbp_msg_callbacks_node_t callback;

  sbp_state_set_io_context(&sbp_state, &context);
  sbp_register_callback(&sbp_state, SBP_MSG_SETTINGS_WRITE, write_callback, state, &callback);

  s8 retval = SBP_OK;
  for( /*empty*/;
       retval == SBP_OK;
       retval = sbp_process(&sbp_state, one_buffer_read))
  {
    continue;
  }

  if (retval != SBP_OK_CALLBACK_EXECUTED) {
    piksi_log(LOG_WARNING, "Error during SBP processing: %d", retval);
  }

  debug_printf("filter: sensitive write reject: %hhd, retval: %hhd\n", state->reject, retval);

  return state->reject;
}

static bool isallspace(const char* s, size_t len)
{
  for (int x = 0; x < len; x++) {
    if (!isspace(s[x]))
      return false;
  }
  return true;
}

static void log_blast(int level, const char* message)
{
  piksi_log(level, message);
  sbp_log(level, message);
  fprintf(stderr, message);
  fprintf(stderr, "\n");
  fflush(stderr);
}

static ssize_t collate_whitelist(const char* whitelist, size_t whitelist_length, whitelist_entry_t *entries, cmph_t* hash)
{
  char entry[512] = {0};

  size_t count = 0;
  size_t offset = 0;

  // Subtract out NULL terminator
  whitelist_length -= 1;

  while (whitelist_length > 0) {

    entry[0] = '\0';
    sscanf(whitelist, "%512[^\n]\n", entry);

    size_t len = strlen(entry);

    size_t previous_offset = offset;
    offset += len + 1/*newline*/; 

    if (len == 0 || isallspace(entry, len)) {
      log_blast(LOG_ERR, MSG_ERROR_EMPTY_LINES);
      return -1;
    }

    char leading_space[512];

    if (sscanf(entry, "%512[ \t\f\v]", leading_space) > 0) {
      log_blast(LOG_ERR, MSG_ERROR_NO_LEADING_SPACES);
      return -2;
    }

    char entry_stripped[512];

    sscanf(entry, "%512[^ \t\f\v]", entry_stripped);

    if (strlen(entry_stripped) != len) {
      log_blast(LOG_ERR, MSG_ERROR_NO_TRAILING_SPACES);
      return -3;
    }

    if (entries != NULL) {

      uint32_t index = cmph_search(hash, entry, len);

      entries[index].offset = previous_offset;
      entries[index].length = len;
    }

    count++;

    whitelist_length -= len + 1;
    whitelist += len + 1;
  }

  return count;
}

void * filter_swl_create(const char *filename)
{
  filter_swl_state_t *s = (filter_swl_state_t *)malloc(sizeof(*s));
  if (s == NULL) {
    return NULL;
  }

  s->keys_fp = fopen(filename, "r");

  fseek(s->keys_fp, 0, SEEK_END);
  s->whitelist_size = ftell(s->keys_fp);

  fseek(s->keys_fp, 0, SEEK_SET);
  
  int fd = fileno(s->keys_fp);
  s->whitelist = ((const char*)
    mmap(NULL, s->whitelist_size, PROT_READ, MAP_SHARED, fd, 0));

  whitelist_entry_t* entries = NULL;
  size_t count = collate_whitelist(s->whitelist, s->whitelist_size, entries, NULL);

  if (count < 0)
    exit(count);

  s->source = cmph_io_nlfile_adapter(s->keys_fp);
  cmph_config_t *config = cmph_config_new(s->source);
  cmph_config_set_algo(config, CMPH_BDZ);
  s->hash = cmph_new(config);
  cmph_config_destroy(config);

  entries = (whitelist_entry_t*) malloc(count * sizeof(whitelist_entry_t));
  collate_whitelist(s->whitelist, s->whitelist_size, entries, s->hash);

  s->whitelist_entries = entries;
  s->whitelist_entry_count = count;

  return (void *)s;
}

void filter_swl_destroy(void **s)
{
  filter_swl_state_t** state = (filter_swl_state_t**) s;

  munmap((void*)(*state)->whitelist, (*state)->whitelist_size);

  cmph_destroy((*state)->hash);

  cmph_io_nlfile_adapter_destroy((*state)->source);
  fclose((*state)->keys_fp);

  free((*state)->whitelist_entries);

  free(*state);
  *state = NULL;
}

int filter_swl_process(void *state, const uint8_t *msg, uint32_t msg_length)
{
  filter_swl_state_t* ctx = (filter_swl_state_t*) state;
  return reject_sensitive_settings_write(ctx, msg, msg_length);
}
