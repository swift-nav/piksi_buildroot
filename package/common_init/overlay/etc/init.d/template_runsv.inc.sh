#!/bin/ash
# shellcheck disable=SC1091,SC2169

# Template for supervised services in /etc/init.d

## Example init script:
#
#   name="foo"
#   cmd="run_foo_svc"
#   dir="/"
#   user="foo_user"
#   group="foo_group"
#   failed_restarts=10
#
#   source /etc/init.d/template_runsv.inc.sh
#
#   setup_permissions() { ... (Optional) configure permissions for service ... }

. /etc/init.d/common.sh

stdout_log="/var/log/$name.log"
stderr_log="/var/log/$name.err"

_setup_svdir()
{
  if [[ -e "/var/service/${name}" ]]; then
    return
  fi

  mkdir -p "/etc/sv/${name}/control"

  if [[ -z "${name:-}" ]]; then
    echo "Error: the 'name' variable must not be empty" >&2
    exit 1
  fi

  if [[ -z "${dir:-}" ]]; then
    dir="/"
  fi

  if [[ -z "${user:-}" ]]; then
    echo "Error: the 'user' variable must not be empty" >&2
    exit 1
  fi

  if [[ -z "${cmd:-}" ]]; then
    echo "Error: the 'cmd' variable must not be empty" >&2
    exit 1
  fi

  if [[ -z "${priority:-}" ]]; then
    priority=0
  fi

  if [[ -z "${group:-}" ]]; then
    group=$user
  fi

  if [[ -z "${failed_restarts:-}" ]]; then
    failed_restarts=10
  fi

  {
    echo "#!/bin/ash"
    echo ""
    echo "exec start_service \\"
    echo "  \"$name\" \"$cmd\" \"$dir\" \"$priority\" \"$user\" \"$group\" \"$stdout_log\" \"$stderr_log\""

  } >"/etc/sv/${name}/run"

  chmod +x "/etc/sv/${name}/run"

  {
    echo "#!/bin/ash"
    echo ""
    echo "run_status=\$1; shift"
    echo ""
    echo "exec service_stopped \"$name\" \"$failed_restarts\" \"\$run_status\""

  } >"/etc/sv/${name}/finish"

  chmod +x "/etc/sv/${name}/finish"

  ln -sf "/etc/sv/${name}" "/var/service/${name}"
}

sv_is_running()
{
  local name=$1; shift
  local svc_dir="/var/service/${name}"

  sv status "$svc_dir" | grep -q "^run"
}

do_start()
{
  _setup_permissions
  _setup_svdir

  configure_logrotate_file "${name}_log" "$stdout_log"
  configure_logrotate_file "${name}_err" "$stderr_log"

  sv start "/var/service/${name}"

  for _ in $(seq 1 10); do
    if sv_is_running "$name"; then
      break
    fi
    sleep 0.1;
  done

  if ! sv_is_running "$name"; then
    echo "Error: service '${name}' failed to start after 1 second..."
  fi
}

do_stop()
{
  sv down "/var/service/${name}"
}

case "$1" in
  start)
    do_start
    ;;
  stop)
    do_stop
    ;;
  restart)
    "$0" stop
    "$0" start
    ;;
  status)
    sv status "/var/service/${name}"
    ;;
  *)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
    ;;
esac

exit 0
