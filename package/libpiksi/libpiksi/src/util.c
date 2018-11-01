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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libpiksi/util.h>
#include <libpiksi/logging.h>

/* from pfwp init.c */
#define IMAGE_HARDWARE_INVALID 0xffffffff
#define IMAGE_HARDWARE_UNKNOWN 0x00000000
#define IMAGE_HARDWARE_V3_MICROZED 0x00000001
#define IMAGE_HARDWARE_V3_EVT1 0x00000011
#define IMAGE_HARDWARE_V3_EVT2 0x00000012
#define IMAGE_HARDWARE_V3_PROD 0x00000013

#define SBP_SENDER_ID_FILE_PATH "/cfg/sbp_sender_id"
#define DEVICE_UUID_FILE_PATH "/cfg/device_uuid"

#define DEVICE_HARDWARE_REVISION_FILE_PATH "/factory/hardware"
#define DEVICE_HARDWARE_REVISION_MAX_LENGTH (64u)
#define DEVICE_HARDWARE_VERSION_FILE_PATH "/factory/hardware_version"
#define DEVICE_HARDWARE_VERSION_MAX_LENGTH (64u)

#define DEVICE_DURO_EEPROM_PATH "/cfg/duro_eeprom"
#define DEVICE_DURO_MAX_CONTENTS_SIZE (128u)
#define DEVICE_DURO_ID_STRING "DUROV0"
#define POSEDAEMON_FILE_PATH "/usr/bin/PoseDaemon"
#define SMOOTHPOSE_LICENSE_FILE_PATH "/persistent/licenses/smoothpose_license.json"
#define DEVICE_DURO_EEPROM_RETRY_INTERVAL_MS 250
#define DEVICE_DURO_EEPROM_RETRY_TIMES 6

#define PROC_UPTIME_FILE_PATH "/proc/uptime"
#define UPTIME_READ_MAX_LENGTH (64u)

#define GPS_TIME_FILE_PATH "/var/run/health/gps_time_available"

#define PIPE_READ_SIDE 0
#define PIPE_WRITE_SIDE 1

void snprintf_assert(char *s, size_t n, const char *format, ...)
{
  assert(s);
  assert(format);

  va_list args;
  va_start(args, format);

  int count = vsnprintf(s, n, format, args);
  assert((size_t)count < n);

  va_end(args);
}

bool snprintf_warn(char *s, size_t n, const char *format, ...)
{
  assert(s);
  assert(format);

  bool ret = true;

  va_list args;
  va_start(args, format);

  int count = vsnprintf(s, n, format, args);
  if ((size_t)count >= n) {
    piksi_log(LOG_WARNING, "snprintf truncation");
    ret = false;
  }

  va_end(args);

  return ret;
}

int file_read_string(const char *filename, char *str, size_t str_size)
{
  if (filename == NULL) {
    piksi_log(LOG_ERR, "%s: filename is NULL", __FUNCTION__);
    return -1;
  }

  if (str == NULL) {
    piksi_log(LOG_ERR, "%s: str is NULL", __FUNCTION__);
    return -1;
  }

  memset(str, 0, str_size);

  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    piksi_log(LOG_ERR, "%s: error opening %s", __FUNCTION__, filename);
    return -1;
  }

  if (fgets(str, str_size, fp) == NULL) {
    fclose(fp);
    piksi_log(LOG_ERR, "%s: error reading %s", __FUNCTION__, filename);
    return -1;
  }

  size_t len = strlen(str);
  /* EOF not reached AND no newline at the str end */
  bool truncated = !feof(fp) && (len != 0 && str[len - 1] != '\n');

  fclose(fp);

  if (truncated) {
    piksi_log(LOG_WARNING, "%s: str was truncated", __FUNCTION__);
  }

  return 0;
}


bool file_read_value(char *file_path)
{
  /* Accommodate also the terminating null char fgets always adds */
  char val_char[2];
  if (file_read_string(file_path, val_char, sizeof(val_char)) != 0) {
    return false;
  }

  return ('1' == val_char[0]);
}

bool file_write_string(const char *filename, const char *str)
{
  FILE *fp = fopen(filename, "w");
  if (fp == NULL) {
    piksi_log(LOG_ERR, "error opening %s: %s", filename, strerror(errno));
    return false;
  }

  bool success = (fputs(str, fp) != EOF);

  fclose(fp);

  if (!success) {
    piksi_log(LOG_ERR, "error reading %s: %s", filename, strerror(errno));
    return false;
  }

  return true;
}

bool file_append_string(const char *filename, const char *str)
{
  FILE *fp = fopen(filename, "a");
  if (fp == NULL) {
    piksi_log(LOG_ERR, "error opening %s: %s", filename, strerror(errno));
    return false;
  }
  bool success = (fprintf(fp, "%s\n", str) > 0);
  fclose(fp);
  if (!success) {
    piksi_log(LOG_ERR, "error writing %s: %s", filename, strerror(errno));
    return false;
  }
  return true;
}

u16 sbp_sender_id_get(void)
{
  u16 sbp_sender_id = SBP_SENDER_ID;

  char sbp_sender_id_string[32];
  if (file_read_string(SBP_SENDER_ID_FILE_PATH, sbp_sender_id_string, sizeof(sbp_sender_id_string))
      == 0) {
    sbp_sender_id = strtoul(sbp_sender_id_string, NULL, 10);
  }

  return sbp_sender_id;
}

u64 system_uptime_ms_get(void)
{
  char uptime_string[UPTIME_READ_MAX_LENGTH];
  u64 uptime_ms = 0;
  if (file_read_string(PROC_UPTIME_FILE_PATH, uptime_string, sizeof(uptime_string)) == 0) {
    uptime_ms = (u64)(1e3 * strtod(uptime_string, NULL));
  }
  return uptime_ms;
}

int device_uuid_get(char *str, size_t str_size)
{
  return file_read_string(DEVICE_UUID_FILE_PATH, str, str_size);
}

bool device_is_duro(void)
{
  char duro_eeprom_sig[DEVICE_DURO_MAX_CONTENTS_SIZE];
  /* DEVICE_DURO_EEPROM_PATH will be created by S18 whether
   * there is EEPROM or not */
  for (int i; i < DEVICE_DURO_EEPROM_RETRY_TIMES; i++) {
    if (access(DEVICE_DURO_EEPROM_PATH, F_OK) == 0) {
      break;
    }
    usleep(DEVICE_DURO_EEPROM_RETRY_INTERVAL_MS * 1000);
  }
  if (file_read_string(DEVICE_DURO_EEPROM_PATH, duro_eeprom_sig, sizeof(duro_eeprom_sig)) != 0) {
    piksi_log(LOG_WARNING, "Failed to read DURO eeprom contents");
    return false;
  }

  return (memcmp(duro_eeprom_sig, DEVICE_DURO_ID_STRING, strlen(DEVICE_DURO_ID_STRING)) == 0);
}

static bool device_has_ins(void)
{
  return (access(POSEDAEMON_FILE_PATH, F_OK) != -1
          && access(SMOOTHPOSE_LICENSE_FILE_PATH, F_OK) != -1);
}

int hw_version_string_get(char *hw_version_string, size_t size)
{
  char raw_hw_ver_string[DEVICE_HARDWARE_VERSION_MAX_LENGTH];
  u64 hw_version = 0;
  if (file_read_string(DEVICE_HARDWARE_VERSION_FILE_PATH,
                       raw_hw_ver_string,
                       sizeof(raw_hw_ver_string))
      == 0) {
    hw_version = (u64)strtod(raw_hw_ver_string, NULL);
    if (hw_version == 0 && errno == EINVAL) {
      piksi_log(LOG_ERR, "Error converting hardware version string: EINVAL");
      return -1;
    }
  } else {
    piksi_log(LOG_ERR,
              "Error reading hardware version string (buffer size: %d)",
              sizeof(raw_hw_ver_string));
    return -1;
  }
  u16 major_ver = hw_version >> 16;
  u16 minor_ver = hw_version & 0xFFFF;
  int written = snprintf(hw_version_string, size, "%d.%d", major_ver, minor_ver);
  if (written < 0) {
    piksi_log(LOG_ERR, "Error writing hardware version string to buffer");
    return -1;
  }
  if (written >= size) {
    piksi_log(
      LOG_ERR,
      "Hardware version string truncated when writing to buffer (size of intended string: %d)",
      written);
    return -1;
  }
  return 0;
}

int hw_revision_string_get(char *hw_revision_string, size_t size)
{
  const char *s = NULL;

  char raw_hw_rev_string[DEVICE_HARDWARE_REVISION_MAX_LENGTH];
  u16 hw_revision = 0;
  if (file_read_string(DEVICE_HARDWARE_REVISION_FILE_PATH,
                       raw_hw_rev_string,
                       sizeof(raw_hw_rev_string))
      == 0) {
    hw_revision = (u16)strtod(raw_hw_rev_string, NULL);
    if (hw_revision == 0 && errno == EINVAL) {
      piksi_log(LOG_ERR, "Error converting hardware revision string: EINVAL");
      return -1;
    }
  } else {
    piksi_log(LOG_ERR,
              "Error reading hardware revision string (buffer size: %d)",
              sizeof(raw_hw_rev_string));
    return -1;
  }

  switch (hw_revision) {
  case IMAGE_HARDWARE_UNKNOWN: s = "Unknown"; break;
  case IMAGE_HARDWARE_V3_MICROZED: s = "Piksi Multi MicroZed"; break;
  case IMAGE_HARDWARE_V3_EVT1: s = "Piksi Multi EVT1"; break;
  case IMAGE_HARDWARE_V3_EVT2: s = "Piksi Multi EVT2"; break;
  case IMAGE_HARDWARE_V3_PROD: s = "Piksi Multi"; break;
  default: s = "Invalid"; break;
  }

  if (strlen(s) >= size) {
    piksi_log(LOG_ERR,
              "Hardware revision string too large for buffer (size of intended string: %d)",
              strlen(s));
    return -1;
  }
  strncpy(hw_revision_string, s, size);
  return 0;
}

int hw_variant_string_get(char *hw_variant_string, size_t size)
{
  const char *s = NULL;

  if (device_is_duro()) {
    if (device_has_ins()) {
      s = "Duro Inertial";
    } else {
      s = "Duro";
    }
  } else {
    s = "Multi";
  }

  if (strlen(s) >= size) {
    piksi_log(LOG_ERR,
              "Hardware variant string too large for buffer (size of intended string: %d)",
              strlen(s));
    return -1;
  }
  strncpy(hw_variant_string, s, size);
  return 0;
}

int product_id_string_get(char *product_id_string, size_t size)
{
  const char *s = NULL;

  int written = snprintf(product_id_string,
                         size,
                         "%s%s",
                         device_is_duro() ? "Duro" : "Piksi Multi",
                         device_has_ins() ? " Inertial" : "");
  if (written < 0) {
    piksi_log(LOG_ERR, "Error writing product id string to buffer");
    return -1;
  }
  if (written >= size) {
    piksi_log(LOG_ERR,
              "Product id string truncated when writing to buffer (size of intended string: %d)",
              written);
    return -1;
  }
  return 0;
}

void set_device_has_gps_time(bool has_time)
{
  static bool had_time = false;
  bool file_updated = false;

  /* React only if the state changes */
  if (has_time != had_time) {
    FILE *fp = fopen(GPS_TIME_FILE_PATH, "w");

    if (fp == NULL) {
      piksi_log(LOG_WARNING | LOG_SBP, "Failed to open %s: errno = %d", GPS_TIME_FILE_PATH, errno);
      return;
    }

    char buffer[] = {(has_time ? '1' : '0'), '\n'};

    if (fwrite(buffer, sizeof(char), sizeof(buffer), fp) != sizeof(buffer)) {
      piksi_log(LOG_WARNING | LOG_SBP, "Failed to write %s", GPS_TIME_FILE_PATH);
    } else {
      file_updated = true;
    }

    fclose(fp);
  }

  /* Keep the file and local variable in sync */
  if (file_updated) {
    had_time = has_time;
    piksi_log(LOG_DEBUG | LOG_SBP, "%s updated with value %u", GPS_TIME_FILE_PATH, has_time);
  }
}

bool device_has_gps_time(void)
{
  if (access(GPS_TIME_FILE_PATH, F_OK) == -1) {
    /* File is not created yet, system is most likely still booting */
    piksi_log(LOG_DEBUG, "%s doesn't exist", GPS_TIME_FILE_PATH);
    return false;
  }

  return file_read_value(GPS_TIME_FILE_PATH);
}

void reap_children(bool debug, child_exit_fn_t exit_handler)
{
  int errno_saved = errno;
  int status;

  pid_t child_pid;

  while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (WIFEXITED(status)) {
      if (exit_handler != NULL) exit_handler(child_pid);
      if (debug) {
        piksi_log(LOG_DEBUG,
                  "%s: child (pid: %d) exit status: %d",
                  __FUNCTION__,
                  child_pid,
                  WEXITSTATUS(status));
      }
    } else if (WIFSIGNALED(status)) {
      if (exit_handler != NULL) exit_handler(child_pid);
      if (debug) {
        piksi_log(LOG_DEBUG,
                  "%s: child (pid: %d) term signal: %d",
                  __FUNCTION__,
                  child_pid,
                  WTERMSIG(status));
      }
    } else if (WIFSTOPPED(status)) {
      if (debug) {
        piksi_log(LOG_DEBUG,
                  "%s: child (pid: %d) stop signal: %d",
                  __FUNCTION__,
                  child_pid,
                  WSTOPSIG(status));
      }
    } else {
      if (debug) {
        piksi_log(LOG_DEBUG,
                  "%s: child (pid: %d) unknown status: %d",
                  __FUNCTION__,
                  child_pid,
                  status);
      }
    }
  }

  errno = errno_saved;
}

void setup_sigchld_handler(void (*handler)(int))
{
  struct sigaction sigchild_sa;

  sigchild_sa.sa_handler = handler;
  sigemptyset(&sigchild_sa.sa_mask);
  sigchild_sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  if ((sigaction(SIGCHLD, &sigchild_sa, NULL) != 0)) {
    piksi_log(LOG_ERR, "error setting up SIGCHLD handler");
    exit(-1);
  }
}

void setup_sigint_handler(void (*handler)(int signum, siginfo_t *info, void *ucontext))
{
  struct sigaction interrupt_sa;

  interrupt_sa.sa_sigaction = handler;
  sigemptyset(&interrupt_sa.sa_mask);
  interrupt_sa.sa_flags = SA_SIGINFO;

  if ((sigaction(SIGINT, &interrupt_sa, NULL) != 0)) {
    piksi_log(LOG_ERR, "error setting up SIGINT handler");
    exit(-1);
  }
}

void setup_sigterm_handler(void (*handler)(int signum, siginfo_t *info, void *ucontext))
{
  struct sigaction terminate_sa;

  terminate_sa.sa_sigaction = handler;
  sigemptyset(&terminate_sa.sa_mask);
  terminate_sa.sa_flags = SA_SIGINFO;

  if ((sigaction(SIGTERM, &terminate_sa, NULL) != 0)) {
    piksi_log(LOG_ERR, "error setting up SIGTERM handler");
    exit(-1);
  }
}

int setup_sigtimedwait(sigwait_params_t *params, int sig, time_t tv_sec)
{
  sigemptyset(&params->waitset);
  sigaddset(&params->waitset, sig);
  sigprocmask(SIG_BLOCK, &params->waitset, NULL);

  update_sigtimedwait(params, tv_sec);

  return 0;
}

int update_sigtimedwait(sigwait_params_t *params, time_t tv_sec)
{
  params->timeout.tv_sec = tv_sec;
  params->timeout.tv_nsec = 0;

  return 0;
}

int run_sigtimedwait(sigwait_params_t *params)
{
  int ret = sigtimedwait(&params->waitset, &params->info, &params->timeout);

  if (-1 == ret && EAGAIN == errno) return 1;

  return 0;
}

bool is_file(int fd)
{
  errno = 0;
  int ret = lseek(fd, 0, SEEK_CUR);
  return !(-1 == ret && errno == ESPIPE);
}

static int _run_with_stdin_file(int fd_stdin,
                                const char *cmd,
                                const char *const argv[],
                                int *stdout_pipe_fd)
{
  int stdout_pipe[2];

  if (pipe(stdout_pipe)) {
    return 1;
  }

  int pid = fork();

  /* Parent */
  if (pid > 0) {

    close(stdout_pipe[PIPE_WRITE_SIDE]);
    *stdout_pipe_fd = stdout_pipe[PIPE_READ_SIDE];

    return 0;
  }

  if (pid == -1) {
    return 1;
  }

  if (fd_stdin >= 0) {

    if (dup2(fd_stdin, STDIN_FILENO) < 0) {
      close(fd_stdin);
      exit(EXIT_FAILURE);
    }

    close(fd_stdin);
  }

  int fd = dup2(stdout_pipe[PIPE_WRITE_SIDE], STDOUT_FILENO);
  if (fd < 0) {
    piksi_log(LOG_ERR, "%s: dup2 failed", __FUNCTION__);
    exit(EXIT_FAILURE);
  }

  close(stdout_pipe[PIPE_READ_SIDE]);

  execvp(cmd, (char *const *)argv);
  piksi_log(LOG_ERR | LOG_SBP,
            "%s: execvp: errno: %s (%s:%d)\n",
            __FUNCTION__,
            strerror(errno),
            __FILE__,
            __LINE__);

  exit(EXIT_FAILURE);
  __builtin_unreachable();
}

int run_command(const run_command_t *r)
{
  return run_with_stdin_file2(r->input,
                              r->argv[0],
                              r->argv,
                              r->buffer,
                              r->length,
                              r->func,
                              r->context);
}

int run_with_stdin_file(const char *input_file,
                        const char *cmd,
                        const char *const argv[],
                        char *output,
                        size_t output_size)
{
  return run_with_stdin_file2(input_file, cmd, argv, output, output_size, NULL, NULL);
}

int run_with_stdin_file2(const char *input_file,
                         const char *cmd,
                         const char *const argv[],
                         char *output,
                         size_t output_size,
                         buffer_fn_t buffer_fn,
                         void *context)
{
  int fd_stdin = -1;

  if (input_file) {
    fd_stdin = open(input_file, O_RDONLY);
    if (fd_stdin == -1) {
      exit(EXIT_FAILURE);
    }
  }

  int stdout_pipe = -1;
  int rc = _run_with_stdin_file(fd_stdin, cmd, (const char *const *)argv, &stdout_pipe);

  if (rc != 0) return rc;

  if (buffer_fn != NULL) {

    size_t read_size = 0;
    size_t read_size_orig = 0;
    do {
      read_size = read_size_orig = read(stdout_pipe, output, output_size);
      //      PK_LOG_ANNO(LOG_DEBUG, "read: %d", read_size_orig);
      assert(read_size <= output_size);
      size_t offset = 0;
      while (read_size > 0) {
        //        PK_LOG_ANNO(LOG_DEBUG, "call buffer_fn...");
        ssize_t consumed = buffer_fn(&output[offset], read_size, context);
        if (consumed < 0) break;
        if (consumed < read_size) {
          offset += consumed;
          read_size -= consumed;
        } else {
          read_size = 0;
        }
        //        PK_LOG_ANNO(LOG_DEBUG, "read_size: %d", read_size);
      }
    } while (read_size_orig > 0);
  } else {
    size_t total = 0;
    while (output_size > total) {
      ssize_t ret = read(stdout_pipe, output + total, output_size - total);
      if (ret > 0) {
        total += ret;
      } else {
        break;
      }
    }
    /* Guarantee null terminated output */
    if (total >= output_size) {
      output[output_size - 1] = 0;
    } else {
      output[total] = 0;
    }
  }

  close(stdout_pipe);

  int status;
  wait(&status);

  return status;
}

static pipeline_t *pipeline_cat(pipeline_t *r, const char *filename)
{
  if (r->_is_nil) return r;

  r->_filename = filename;
  return r;
};

static pipeline_t *pipeline_pipe(pipeline_t *r)
{
  if (r->_is_nil) return r;

  if (r->_filename != NULL) {

    int fd = open(r->_filename, O_RDONLY);

    if (fd < 0) {
      piksi_log(LOG_ERR,
                "%s: error opening file: %s (%s:%d)",
                __FUNCTION__,
                strerror(errno),
                __FILE__,
                __LINE__);
      r->_is_nil = true;
      return r;
    }

    r->_filename = NULL;
    r->_fd_stdin = fd;

  } else if (r->_proc_path != NULL) {

    int out_fd = -1;
    int rc = _run_with_stdin_file(r->_fd_stdin, r->_proc_path, r->_proc_args, &out_fd);

    assert(r->_closeable_idx < COUNT_OF(r->_closeable_fds));
    r->_closeable_fds[r->_closeable_idx++] = r->_fd_stdin;

    if (rc != 0) {
      r->_is_nil = true;
      return r;
    }

    r->_fd_stdin = out_fd;

    r->_proc_path = NULL;
    r->_proc_args = NULL;

  } else {

    piksi_log(LOG_ERR, "%s nothing staged for pipe (%s:%d)", __FUNCTION__, __FILE__, __LINE__);
    r->_is_nil = true;
  }

  return r;
};

static pipeline_t *pipeline_call(pipeline_t *r, const char *proc_path, const char *const argv[])
{
  if (r->_is_nil) return r;

  size_t index = 0;

  r->_proc_path = proc_path;
  r->_proc_args = argv;

  return r;
};

static pipeline_t *pipeline_wait(pipeline_t *r)
{
  if (r->_is_nil) return r;

  int out_fd = -1;
  int rc = _run_with_stdin_file(r->_fd_stdin, r->_proc_path, r->_proc_args, &out_fd);

  assert(r->_closeable_idx < COUNT_OF(r->_closeable_fds));
  r->_closeable_fds[r->_closeable_idx++] = r->_fd_stdin;

  if (rc != 0) {
    r->_is_nil = true;
    return r;
  }

  r->_fd_stdin = out_fd;

  r->_proc_path = NULL;
  r->_proc_args = NULL;

  size_t total = 0;
  size_t output_size = sizeof(r->stdout_buffer);

  char *output = r->stdout_buffer;

  while (output_size > total) {
    ssize_t ret = read(out_fd, output + total, output_size - total);
    if (ret > 0) {
      total += ret;
    } else {
      break;
    }
  }

  /* Guarantee null terminated output */
  if (total >= output_size) {
    output[output_size - 1] = 0;
  } else {
    output[total] = 0;
  }

  close(out_fd);

  int status;
  wait(&status);

  r->exit_code = status;

  return r;
};

static bool pipeline_is_nil(pipeline_t *r)
{
  return r->_is_nil;
};

static pipeline_t *pipeline_destroy(pipeline_t *r)
{
  if (r->_fd_stdin >= 0) {
    close(r->_fd_stdin);
    r->_fd_stdin = -1;
  }
  for (size_t x = 0; x < COUNT_OF(r->_closeable_fds); x++) {
    if (r->_closeable_fds[x] == -1) break;
    close(r->_closeable_fds[x]);
  }
  free(r);
  return NULL;
};

pipeline_t *create_pipeline(void)
{
  pipeline_t *pipeline = calloc(1, sizeof(pipeline_t));

  *pipeline = (pipeline_t){
    .cat = pipeline_cat,
    .pipe = pipeline_pipe,
    .call = pipeline_call,
    .wait = pipeline_wait,
    .is_nil = pipeline_is_nil,
    .destroy = pipeline_destroy,

    .stdout_buffer = {0},
    .exit_code = EXIT_FAILURE,

    ._is_nil = false,
    ._filename = NULL,
    ._fd_stdin = -1,
    ._proc_path = NULL,
    ._proc_args = NULL,
    ._closeable_fds = {-1},
    ._closeable_idx = 0,
  };

  return pipeline;
}

bool str_digits_only(const char *str)
{
  if (str == NULL) {
    return false;
  }

  /* Empty string */
  if (!(*str)) {
    return false;
  }

  while (*str) {
    if (isdigit(*str++) == 0) {
      return false;
    }
  }

  return true;
}

bool strtoul_all(int base, const char *str, unsigned long *value)
{
  char *endptr = NULL;
  *value = strtoul(str, &endptr, base);
  while (isspace(*endptr))
    endptr++;
  if (*endptr != '\0') {
    return false;
  }
  if (str == endptr) {
    return false;
  }
  return true;
};

bool strtod_all(const char *str, double *value)
{
  char *endptr = NULL;
  *value = strtod(str, &endptr);
  while (isspace(*endptr))
    endptr++;
  if (*endptr != '\0') {
    return false;
  }
  if (str == endptr) {
    return false;
  }
  return true;
};
