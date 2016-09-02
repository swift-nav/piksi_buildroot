# /etc/init.d template for simple commands

# name=""
# start() { }
# stop() { }
# source template.inc.sh

run_start() {
    echo "Starting $name"
    start
}

run_stop() {
    echo "Stopping $name"
    stop
}

case "$1" in
  start)
    run_start
    ;;
  stop)
    run_stop
    ;;
  restart|reload)
    run_stop
    run_start
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
esac

exit 0

