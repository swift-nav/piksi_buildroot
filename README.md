# Piksi Multi Buildroot

[![Build Status](https://travis-ci.org/swift-nav/piksi_buildroot.svg?branch=master)](https://travis-ci.org/swift-nav/piksi_buildroot)

[Buildroot](https://buildroot.org/) configuration and packages for building the Piksi Multi system image.

## Overview

`piksi_buildroot` is a Buildroot project targeting the Piksi Multi GNSS receiver. It consists of
- Buildroot, as a git submodule
- An external Buildroot tree in the top-level `piksi_buildroot` directory:
  - Buildroot configuration in `configs`
  - Custom packages in `package`
  - Board-specific files (rootfs overlay, device trees, scripts, etc.) in `board/piksiv3`


## Fetching firmware binaries

To build a production system image, the build process expects the following
firmware and FPGA binaries to be present:

```
firmware/prod/piksi_firmware.elf
firmware/prod/piksi_fpga.bit
firmware/microzed/piksi_firmware.elf
firmware/microzed/piksi_fpga.bit
```

You can use the following command to download these binaries from S3. Note that
this requires `awscli` to be installed and AWS credentials to be
properly configured.

``` sh
make firmware
```

Check `fetch-firmware.sh` to see which image versions are being used.

Note that these binaries are only used by the production system image. In the
development system image they are instead read from the network or SD
card.

## Building

### Linux Native

Ensure you have the dependencies. See `Dockerfile` for build dependencies.

Run

``` sh
# Initialize submodules
git submodule update --init --recursive
# Download firmware and FPGA binaries
make firmware
# Build the system image
make image
```

Images will be in the `buildroot/output/images` folder.

### Docker

Install [Docker](https://docs.docker.com/engine/installation/#platform-support-matrix) for your platform.

Run

``` sh
# Initialize submodules
git submodule update --init --recursive
# Download firmware and FPGA binaries
make firmware
# Set up Docker image
make docker-setup
# Build the system image in a Docker container
make docker-make-image
```

Images will be in the `buildroot/output/images` folder.

## Incremental Builds

It is possible to rebuild individual packages and regenerate the system image. Note that Buildroot does _not_ automatically rebuild dependencies or handle configuration changes. In some cases a full rebuild may be necessary. See the [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html) for details.

### Linux Native

``` sh
# Enter buildroot directory
cd buildroot
# Set BR2_EXTERNAL to the piksi_buildroot directory
export BR2_EXTERNAL=..
# Rebuild a package
make libpiksi-rebuild
# Rebuild the system image
make image
```

### Docker

#### Build shell

``` sh
# Start interactive Docker container
make docker-run
# Rebuild a package
make -C buildroot libpiksi-rebuild
# Rebuild the system image
make image
```

#### One-shot make target

To do an incremental rebuild of a package, invoke the following:

``` sh
make docker-pkg-ntrip_daemon-rebuild
```

To get an idea of what other commands are available for a package, inspect
the help output from buildroot:

```sh
# Launch the build shell
make docker-run
# Ask buildroot for help
make -C buildroot help
```

## Copying data out of docker

For speed, most buildroot data is captured in side a docker volume.  The volume
keeps file access inside a docker container instead of moving the data from
container to host (which is slow on many platforms, except Linux).

The following diagram shows how Docker composes the file system layers we
specify for the build container:

<pre>
  ┌────────────────────────────────────────────────┐
  │                                                │
  │ Host Filesystem                                │          Initially
  │   ($PWD mapped to /piksi_buildroot)            │       populated with
  │                                                │         contents of
  └──────┬─────────────────────────────────────────┘ ┌──── $PWD/buildroot,
         │                                           │    changes are only
         │        ┌──────────────────────────────┐   │       visible in
         │        │  Docker Volume:              │   │         docker.
         ├───────▶│    (/buildroot/)             │◀──┘
         │        └──────────────────────────────┘
         │                                               Anything written
         │        ┌──────────────────────────────┐       here will show up
         │        │ Container to host mapping:   │  ┌───    in the host
         ├───────▶│   (/buildroot/output/images) │◀─┘       filesystem.
         │        └──────────────────────────────┘
         │
         │        ┌──────────────────────────────┐
         │        │ Ephemeral container layer    │       Anything that doesn't
         └───────▶│                              │◀─┐      match an existing
                  └──────────────────────────────┘  │       mapping will be
                                                    └───    captured in an
                                                          ephemeral (per run)
                                                                layer.
</pre>

To copy data out of docker, use the following make rule:

```
make docker-cp SRC=/piksi_buildroot/buildroot/output/target/usr/bin/nap_linux DST=/tmp/nap_linux
```
