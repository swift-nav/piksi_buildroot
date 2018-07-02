#!/bin/ash

[[ -n "$name"    ]] || { echo "ERROR: 'name' variable is not defined" >&2; exit 1; }
[[ -n "$service" ]] || { echo "ERROR: 'service' variable is not defined" >&2; exit 1; }
[[ -n "$user"    ]] || { echo "ERROR: 'user' variable is not defined" >&2; exit 1; }

stderr=/var/run/fifos/${service}.stderr
stdout=/var/run/fifos/${service}.stdout

cleanup()
{
  rm -f $stderr
  rm -f $stdout

  kill -HUP $pid $stderr_pid $stdout_pid
  kill -TERM $pid $stderr_pid $stdout_pid

  exit 0
}

trap 'cleanup' HUP TERM STOP EXIT

rm -f $stderr $stdout
mkfifo $stderr $stdout

chpst -u pk_log logger -t ${service} -p daemon.info <$stdout &
stdout_pid=$!

chpst -u pk_log logger -t ${service} -p daemon.err <$stderr &
stderr_pid=$!

mkdir -p /var/run/${name}/sv
chown -R ${user}:${user} /var/run/${name}

chpst -u ${user} runsvdir /var/run/${name}/sv 1>$stdout 2>$stderr &

pid=$!
wait $pid
