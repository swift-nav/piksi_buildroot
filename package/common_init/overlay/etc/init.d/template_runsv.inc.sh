#!/bin/ash

# /etc/init.d template for processes

# name=""
# cmd=""
# dir="/"
# user=""
# source template.inc.sh

source /etc/init.d/common.sh

[[ -n "$name" ]] || {
  echo "Error: the 'name' variable must not be empty" >&2
  exit 1
}

stdout_log="/var/log/$name.log"
stderr_log="/var/log/$name.err"

tag=tmpl_runsv
fac=daemon

_setup_svdir()
{
  if [[ -e "/var/service/${name}" ]]; then
    return
  fi

  mkdir -p "/etc/sv/${name}/control"

  if [[ -z "$user" ]]; then
    echo "Error: the 'user' variable must not be empty"
    exit 1
  fi

  if [[ -z "$priority" ]]; then
    priority=0
  fi

  if [[ -z "$group" ]]; then
    group=$user
  fi

  echo "#!/bin/ash"                                            > "/etc/sv/${name}/run"
  echo "cd ${dir}"                                            >> "/etc/sv/${name}/run"
  echo "echo Starting ${name}... \\"                          >> "/etc/sv/${name}/run"
  echo "  | logger -t ${tag} -p ${fac}.info"                  >> "/etc/sv/${name}/run"
  echo "exec nice -n $priority chpst -u $user:$group $cmd \\" >> "/etc/sv/${name}/run"
  echo "  1>>${stdout_log} \\"                                >> "/etc/sv/${name}/run"
  echo "  2>>${stderr_log}"                                   >> "/etc/sv/${name}/run"

  chmod +x /etc/sv/${name}/run

  echo "#!/bin/ash"                                  > "/etc/sv/${name}/finish"
  echo "echo Service ${name} exited... \\"          >> "/etc/sv/${name}/finish"
  echo "  | logger -t ${tag} -p ${fac}.info"        >> "/etc/sv/${name}/finish"
  echo "sleep 1"                                    >> "/etc/sv/${name}/finish"

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
