log_fac=daemon

logw() { logger -t $log_tag -p ${log_fac}.warn  $*; }
logi() { logger -t $log_tag -p ${log_fac}.info  $*; }
logd() { logger -t $log_tag -p ${log_fac}.debug $*; }
loge() { logger -t $log_tag -p ${log_fac}.err   $*; }

setup_loggers()
{
  logger_stdout=$(mktemp -u)
  mkfifo $logger_stdout

  logger_stderr=$(mktemp -u)
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
