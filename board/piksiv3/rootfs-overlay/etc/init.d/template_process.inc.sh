# /etc/init.d template for processes

# name=""
# cmd=""
# dir="/"
# user=""
# source template.inc.sh

source /etc/init.d/common.sh

ulimit -c unlimited

pid_file="/var/run/$name.pid"
stdout_log="/var/log/$name.log"
stderr_log="/var/log/$name.err"

do_start()
{
  if is_running $pid_file; then
    echo "Already started"
  else
    echo "Starting $name"
    _setup_permissions
    cd "$dir"
    if [ -z "$user" ]; then
      sudo $cmd >> "$stdout_log" 2>> "$stderr_log" &
    else
      sudo -u "$user" $cmd >> "$stdout_log" 2>> "$stderr_log" &
    fi
    echo $! > "$pid_file"
    if ! is_running $pid_file; then
      echo "Unable to start, see $stdout_log and $stderr_log"
      exit 1
    fi
  fi
}

do_stop()
{
  if is_running $pid_file; then

    echo -n "Stopping $name.."
    kill $(get_pid $pid_file)

    for i in `seq 1 10`; do
      if ! is_running $pid_file; then
        break
      fi
      echo -n "."
      sleep 1
    done

    echo

    if is_running $pid_file; then
      echo "Not stopped; may still be shutting down or shutdown may have failed"
      exit 1
    else
      echo "Stopped"
      if [ -f "$pid_file" ]; then
        rm "$pid_file"
      fi
    fi
  else
    echo "Not running"
  fi
}

do_restart()
{
  $0 stop
  if is_running $pid_file; then
    echo "Unable to stop, will not attempt to start"
    exit 1
  fi
  $0 start
}

do_status()
{
  if is_running $pid_file; then
    echo "Running"
  else
    echo "Stopped"
    exit 1
  fi
}

case "$1" in
  start)
    do_start
    ;;
  stop)
    do_stop
    ;;
  restart)
    do_restart
    ;;
  status)
    do_status
    ;;
  *)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
    ;;
esac

exit 0
