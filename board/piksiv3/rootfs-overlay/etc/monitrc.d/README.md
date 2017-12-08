Additional config resource for monit can go in this directory in order to
dynamically add a service that monit should manage.

For example:

```
cat >/etc/monitrc.d/sample_daemon.monitrc <<EOF

check process sample_daemon with pidfile /var/run/sample_daemon.pid
    start program = "/etc/init.d/S83sample_daemon start"
    stop program = "/etc/init.d/S83sample_daemon stop"
EOF

monit start sample_daemon
```
