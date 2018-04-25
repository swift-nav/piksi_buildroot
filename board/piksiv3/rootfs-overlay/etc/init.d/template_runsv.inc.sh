# /etc/init.d template for processes

# name=""
# cmd=""
# dir="/"
# user=""
# source template.inc.sh

source /etc/init.d/common.sh

stdout_log="/var/log/$name.log"
stderr_log="/var/log/$name.err"

tag=tmpl_runsv
fac=daemon

pid_file="/var/service/${name}/supervise/pid"

_setup_svdir()
{
  mkdir -p /etc/sv/${name}/control

  if [[ -z "$user" ]]; then
    echo "Error: the 'user' variable must not be empty"
    exit 1
  fi

  echo "#!/bin/ash"                                   > /etc/sv/${name}/run
  echo "cd ${dir}"                                   >> /etc/sv/${name}/run
  echo "echo Starting ${name}... \\"                 >> /etc/sv/${name}/run
  echo "  | logger -t ${tag} -p ${fac}.info"         >> /etc/sv/${name}/run
  echo "exec chpst -u $user:$user $cmd \\"           >> /etc/sv/${name}/run
  echo "  1>>${stdout_log} \\"                       >> /etc/sv/${name}/run
  echo "  2>>${stderr_log}"                          >> /etc/sv/${name}/run

  chmod +x /etc/sv/${name}/run

  echo "#!/bin/ash"                                   > /etc/sv/${name}/finish
  echo "echo Service ${name} exited... \\"           >> /etc/sv/${name}/finish
  echo "  | logger -t ${tag} -p ${fac}.info"         >> /etc/sv/${name}/finish
  echo "sleep 1"                                     >> /etc/sv/${name}/finish

  chmod +x /etc/sv/${name}/finish

  ln -sf /etc/sv/${name} /var/service/${name}
}

case "$1" in
    start)
    _setup_permissions
    _setup_svdir
    sv up /var/service/${name}
    ;;
    stop)
    sv down /var/service/${name}
    ;;
    restart)
    sv down /var/service/${name}
    sv up /var/service/${name}
    ;;
    status)
    sv status /var/service/${name}
    ;;
    *)
    echo "Usage: $0 {start|stop|restart|status}"
    exit 1
    ;;
esac

exit 0

