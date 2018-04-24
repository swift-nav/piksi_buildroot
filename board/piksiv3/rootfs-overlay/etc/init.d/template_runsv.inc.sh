# /etc/init.d template for processes

# name=""
# cmd=""
# dir="/"
# user=""
# source template.inc.sh

stdout_log="/var/log/$name.log"
stderr_log="/var/log/$name.err"

tag=tmpl_runsv
fac=daemon

_setup_permissions() {
    if type setup_permissions | grep -q "shell function"; then
        setup_permissions
    fi
}

pid_file="/var/service/${name}/supervise/pid"

_setup_svdir() {

  mkdir -p /etc/sv/${name}/control

  echo "#!/bin/ash"                                   > /etc/sv/${name}/run
  echo "set -m"                                      >> /etc/sv/${name}/run
  echo "echo Starting ${name}... \\"                 >> /etc/sv/${name}/run
  echo "  | logger -t ${tag} -p ${fac}.info"         >> /etc/sv/${name}/run
  echo "exec chpst -u $user:$user sh -c \"$cmd\" \\" >> /etc/sv/${name}/run
  echo "  1>>${stdout_log} \\"                       >> /etc/sv/${name}/run
  echo "  2>>${stderr_log}"                          >> /etc/sv/${name}/run

  chmod +x /etc/sv/${name}/run

  echo "#!/bin/ash"                                   > /etc/sv/${name}/finish
  echo "echo Service ${name} exited... \\"           >> /etc/sv/${name}/finish
  echo "  | logger -t ${tag} -p ${fac}.info"         >> /etc/sv/${name}/finish

  chmod +x /etc/sv/${name}/finish

  echo "#!/bin/ash"                                   > /etc/sv/${name}/control/d
  echo "echo Stopping ${name}... \\"                 >> /etc/sv/${name}/control/d
  echo "  | logger -t ${tag} -p ${fac}.info"         >> /etc/sv/${name}/control/d
  echo "kill -TERM -\$(cat $pid_file)"               >> /etc/sv/${name}/control/d

  chmod +x /etc/sv/${name}/control/d

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

