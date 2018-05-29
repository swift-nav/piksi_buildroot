# Skylark Upload and Download Daemons

libnetwork is a curl-based library used for publishing and subscribing SBP data
via Piksi's Linux core. We use it in two SBP-over-HTTP daemon services that can
be optionally enabled and monitored on the Piksi: `skylark_daemon` (which operates
in upload or download mode via the --upload or --download switch).

## Why

Provide direct network connectivity to Skylark services. Historical connectivity
has been provided through the serial port either through the Piksi Console or
other utilities. Moving to a direct connection to Skylark services will remove a
lot of troublesome intermediaries, especially the Piksi Console, and hopefully
provide simpler and more robust network connectivity. This will completely
remove the need for external, host-provided network connectivity.

## How

The `skylark_daemon` listens for Skylark configuration and starts and stops
itself in upload and download mode as necessary. The upload and download daemons
run independently for improved robustness and simplicity, pulling and pushing
SBP data to two Skylark-specific ZeroMQ ports that are exposed on the Linux host
are routed to the firmware: `tcp://127.0.0.1:43080` and `tcp://127.0.0.1:43081`,
respectively. Taken together, these run with:

```
mkfifo /var/run/skylark/download /var/run/skylark/upload
skylark_daemon --download --file /var/run/skylark/download --url https://broker.skylark2.swiftnav.com
skylark_daemon --upload --file /var/run/skylark/upload --url https://broker.skylark2.swiftnav.com
endpoint_adapter --file /var/run/skylark/upload -s >tcp://127.0.0.1:43070
endpoint_adapter --file /var/run/skylark/download -p >tcp://127.0.0.1:43071
```

The upload and download modes read and write from two FIFOs they materialize:
`/var/run/skylark/download` and `/var/run/skylark/upload`.  The ZMQ adapter
processes manage the piping and framing of SBP via these pipes.  The dataflow
here looks something like this:

```
skylark_daemon (--download mode):
  HTTP GET => callback writer => /var/run/skylark/download (FIFO) => ZMQ to Piksi Firmware

skylark_daemon (--upload mode):
  ZMQ from Piksi Firmware => /var/run/skylark/upload (FIFO) => callback reader => HTTP PUT
```

### Settings

The `skylark_daemon` will continusouly listen for `skylark.enable` settings
and will start or stop upload and download processes as necessary. The piksi
system daemon will also continuously listen for `skylark.url` settings and will
restart upload and download processes as necessary, but will use a default
Skylark URL in the setting's absence - this default Skylark URL will not be
exposed to users.

### Convenience files maintained in /var/run/skylark

The skylark daemon maintains the file `/var/run/skylark/enabled` for other
daemons and scripts to read in order to easily ascertain if the Skylark download
mode is enabled or not. This is because multiple options (in addition to just
`skylark.enable`) need to be validated before Skylark download mode is
functional.

### Permissions

The `/var/run/skylark` directory and all contained files are owned by the
`skylark_daemon` user. The control path `/var/run/skylark/control` has the
sticky bit set so that any other daemon on the system can request that the
`skylark_daemon` perform certain actions, currently "reconnect" is the only
supported action, running:

    skylark_daemon --reconnect-dl
    
Will cause the daemon to immediately reconnect to the Skylark network.  This
is useful during network start-up scenarios such as when a new network interface
comes online (e.g. the cell network).
