[[ -n "$log_tag" ]] || { echo "ERROR: 'log_tag' variable is not defined" >&2; exit 1; }

log_fac=daemon

log_base()
{
  local send_sbp=
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --sbp) send_sbp=y; shift;;
      *) break;;
    esac
  done
  [[ -z "$send_sbp" ]] || { echo $* | sbp_log --"${_log_level}"; }
  logger -t $log_tag -p ${log_fac}.${_log_level} $*;
}

logw() { _log_level=warn;  log_base $*; }
logi() { _log_level=info;  log_base $*; }
logd() { _log_level=debug; log_base $*; }
loge() { _log_level=err;   log_base $*; }

setup_loggers()
{
  logger_stdout=/var/run/$log_tag.stdout
  rm -f $logger_stdout
  mkfifo $logger_stdout

  logger_stderr=/var/run/$log_tag.stderr
  rm -f $logger_stderr
  mkfifo $logger_stderr

  logger -t $log_tag -p ${log_fac}.warn <$logger_stderr &
  logger_stderr_pid=$!

  logger -t $log_tag -p ${log_fac}.info <$logger_stdout &
  logger_stdout_pid=$!

  exec 1>$logger_stdout
  exec 2>$logger_stdout
}

cleanup_loggers()
{
  kill ${logger_stderr_pid} ${logger_stdout_pid}
  rm ${logger_stdout} ${logger_stderr}
}

setup_loggers
trap 'cleanup_loggers' EXIT TERM
