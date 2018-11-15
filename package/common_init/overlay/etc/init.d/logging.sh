#!/bin/ash
# shellcheck disable=SC1091,SC2169

[[ -n "${log_tag:-}" ]] || { echo "ERROR: 'log_tag' variable is not defined" >&2; exit 1; }

log_fac=daemon

logger_stdout=/var/run/fifos/$log_tag.stdout
logger_stderr=/var/run/fifos/$log_tag.stderr

log_base()
{
  local send_sbp=
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --sbp) send_sbp=y; shift;;
      *) break;;
    esac
  done
  [[ -z "$send_sbp" ]] || { echo "$@" | sbp_log --"${_log_level}"; }
  logger -t "$log_tag" -p "${log_fac}.${_log_level}" "$@"
}

logw() { _log_level=warn;  log_base "$@"; }
logi() { _log_level=info;  log_base "$@"; }
logd() { _log_level=debug; log_base "$@"; }
loge() { _log_level=err;   log_base "$@"; }

_cleanup_logger_fifos()
{
  rm -f "$logger_stdout"
  rm -f "$logger_stderr"
}

_setup_loggers()
{
  _cleanup_logger_fifos

  mkfifo "$logger_stdout"
  mkfifo "$logger_stderr"

  if [[ $(id -u) -eq 0 ]]; then
    chpst -u pk_log logger -t "$log_tag" -p "${log_fac}.warn" <"$logger_stderr" &
  else
    logger -t "$log_tag" -p "${log_fac}.warn" <"$logger_stderr" &
  fi
  logger_stderr_pid=$!

  if [[ $(id -u) -eq 0 ]]; then
    chpst -u pk_log logger -t "$log_tag" -p "${log_fac}.info" <"$logger_stdout" &
  else
    logger -t "$log_tag" -p "${log_fac}.info" <"$logger_stdout" &
  fi
  logger_stdout_pid=$!

  exec 1>"$logger_stdout"
  exec 2>"$logger_stderr"
}

cleanup_loggers()
{
  _cleanup_logger_fifos

  [[ -z "$logger_stderr_pid" ]] || \
    kill ${logger_stderr_pid} &>/dev/null

  [[ -z "$logger_stdout_pid" ]] || \
    kill ${logger_stdout_pid} &>/dev/null
}

setup_loggers()
{
  _setup_loggers
  trap 'cleanup_loggers; exit 0' EXIT TERM HUP
}
