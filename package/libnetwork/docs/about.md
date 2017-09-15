# Skylark Upload and Download Daemons

libnetwork is a curl-based library used for publishing and subscribing SBP data
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

The piksi system daemon listens for Skylark configuration and starts and stops
upload and download daemons as necessary. The upload and download daemons run
independently for improved robustness and simplicity, pulling and pushing SBP
data to two Skylark-specific ZeroMQ ports that are exposed on the Linux host
are routed to the firmware: `tcp://127.0.0.1:43080` and
`tcp://127.0.0.1:43081`, respectively. Taken together, these run with:

```
mkfifo /var/run/skylark_download /var/run/skylark_upload
skylark_download_daemon --file /var/run/skylark_download --url https://broker.skylark2.swiftnav.com
skylark_upload_daemon --file /var/run/skylark_upload --url https://broker.skylark2.swiftnav.com
zmq_adapter --file /var/run/skylark_upload -s >tcp://127.0.0.1:43070
zmq_adapter --file /var/run/skylark_download -p >tcp://127.0.0.1:43071
```

The upload and download daemons read and write from two FIFOs they materialize:
`/var/run/skylark_download` and `/var/run/skylark_upload`.  The ZMQ adapter
processes manage the piping and framing of SBP via these pipes.  The dataflow
here looks something like this:

```
skylark_download_daemon:
  HTTP GET => callback writer => /var/run/skylark_download (FIFO) => ZMQ to Piksi Firmware

skylark_upload_daemon:
  ZMQ from Piksi Firmware => /var/run/skylark_upload (FIFO) => callback reader => HTTP PUT
```

### Settings

The piksi system daemon will continusouly listen for `skylark.enable` settings
and will start or stop upload and download processes as necessary. The piksi
system daemon will also continuously listen for `skylark.url` settings and will
restart upload and download processes as necessary, but will use a default
Skylark URL in the setting's absence - this default Skylark URL will not be
exposed to users.
