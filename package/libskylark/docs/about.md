# Skylark Upload and Download Daemons

libskylark is a curl-based library used for publishing and subscribing SBP data
via Piksi's Linux core. We use it in two SBP-over-HTTP daemon services that can
be optionally enabled and monitored on the Piksi: `skylark_upload_daemon` and
`skylark_download_daemon`.

## Why

Provide direct network connectivity to Skylark services. Historical connectivity
has been provided through the serial port either through the Piksi Console or
other utilities. Moving to a direct connection to Skylark services will remove a
lot of troublesome intermediaries, especially the Piksi Console, and hopefully
provide simpler and more robust network connectivity. This will completely
remove the need for external, host-provided network connectivity.

## How

Setup upload and download daemons that are monitored and continuously run on
Piksi's Linux core - if either process crashes, they will be restarted by
monit. The upload and download daemons run independently for improved robustness
and simplicity, pulling and pushing SBP data to two Skylark-specific ZeroMQ
ports that are exposed on the Linux host on are routed to the firmware:
`tcp://127.0.0.1:43070` and `tcp://127.0.0.1:43071`, respectively. Taken
together, these run with:

```
mkfifo /tmp/skylark_download /tmp/skylark_upload
skylark_download_daemon --pub /tmp/skylark_download --endpoint https://broker.skylark2.swiftnav.com
skylark_upload_daemon --sub /tmp/skylark_upload --endpoint https://broker.skylark2.swiftnav.com
zmq_adapter --file /tmp/skylark_upload -s >tcp://127.0.0.1:43070
zmq_adapter --file /tmp/skylark_download -p >tcp://127.0.0.1:43071
```

The upload and download daemons read and write from two FIFOs
(`/tmp/skylark_download` and `/tmp/skylark_upload`) they materialize. The ZMQ
adapter processes manage the piping and framing of SBP via these pipes. The
dataflow here looks something like this:

```
skylark_download_daemon:
  HTTP GET => callback writer => /tmp/skylark_download (FIFO) => ZMQ to Piksi Firmware

skylark_upload_daemon:
  ZMQ from Piksi Firmware => /tmp/skylark_upload (FIFO) => callback reader => HTTP PUT
```

### Linux Process Management

In `$PIKSI_BUILDROOT/board/piksiv3/rootfs-overlay/etc/init.d/` we define several
init.d scripts start processes on boot. They are started in sequential order:

```
$ ls /etc/init.d

S86skylark0_setup
S86skylark1_upload_daemon
S86skylark2_download_daemon
S86skylark3_zmq_adapter_upload_daemon
S86skylark4_zmq_adapter_download_daemon
```

In `$PIKSI_BUILDROOT/board/piksiv3/rootfs-overlay/etc/monitrc` we configure
monit to restart the last four process should they exit with a failure code.

### Settings

Upload and download daemons will continuously listen for `SKYLARK_ENABLE`
settings and will start or stop their child processes as necessary. Upload and
download daemons will also continuously listen for `SKYLARK_URL` settings and
will update the restart their child processes as necessary, but will use a
default Skylark URL in the setting's absence.

### Upload

The upload daemon maintains a master process that continuously listens for
Skylark settings and creates child processes to receive SBP messages from the
FIFO and send SBP messages to Skylark with libcurl - this should require two
child processes for zeromq and libcurl. The two child processes exchange data -
the libcurl process sends messages from the zeromq process - and will use a pipe
to exchange data between processes.

### Download

The download daemon will maintain a master process that continuously listens for
Skylark settings and will create a child process to receive SBP messages from
Skylark with libcurl and send SBP messages to the FIFO - this should require one
child process for libcurl.
