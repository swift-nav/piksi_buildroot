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

## Building
<u>Note:</u>
Images built from source are no longer compatible with official releases. In order to
upgrade to the latest official release you must install the pre-built
[v2.0.2](https://github.com/swift-nav/piksi_buildroot/releases/tag/v2.0.2) binary.

### Docker

Install [Docker](https://docs.docker.com/engine/installation/#platform-support-matrix) for your platform.

Run

``` sh
# Set up Docker image
make docker-setup
# Download firmware and FPGA binaries
make docker-make-firmware
# Build the system image in a Docker container
make docker-make-image
```

Images will be in the `buildroot/output/images` folder.

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

### Build Outputs

A successfull build should finish with the following output visible:

``` sh
>>>   Executing post-image script /piksi_buildroot/board/piksiv3/post_image.sh
>>> Generating FAILSAFE firmware image... done.
>>> Generating DEV firmware image... done.
>>> Generating PROD firmware image... done.
>>> INTERNAL firmware image located at:
        buildroot/output/images/piksiv3_prod/PiksiMulti-INTERNAL-<version_tag>.bin
>>> DEV firmware image located at:
        buildroot/output/images/piksiv3_prod/PiksiMulti-DEV-<version_tag>.bin
>>> FAILSAFE firmware image located at:
        buildroot/output/images/piksiv3_prod/PiksiMulti-FAILSAFE-<version_tag>.bin
```

The build variants are as follows:
 * `INTERNAL` is a complete image with the firmware and FPGA binaries included.
 * `DEV` is a minimal u-boot image that is configured for loading development artifacts.
 * `FAILSAFE` is an even more minimal u-boot image that allows for manual recovery.

Without prior experience and instructions, it is recommended that the `DEV` and `FAILSAFE`
images be ignored.

A `PiksiMulti-*.bin` binary can be loaded onto the device using the console, or via
usb thumbdrive auto-upgrade feature.

## Incremental Builds

It is possible to rebuild individual packages and regenerate the system image. Note that Buildroot does _not_ automatically rebuild dependencies or handle configuration changes. In some cases a full rebuild may be necessary. See the [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html) for details.

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
make docker-pkg-<package_name>-rebuild
```

Where `<package_name>` is a package name like `ntrip_daemon`.  To get an idea
of what other commands are available for a package, inspect the help output
from buildroot:

```sh
# Launch the build shell
make docker-run
# Ask buildroot for help
make -C buildroot help
```

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

## Building the sample daemon

In order to build the sample daemon located [here](package/sample_daemon), set
the `BR2_BUILD_SAMPLE_DAEMON` environement variable to `y` in the build
environment:

```
export BR2_BUILD_SAMPLE_DAEMON=y
```

Then build normally:
``` sh
# Docker
make docker-make-image

# or, Linux native
make image
```

## Fetching firmware binaries

To build a whole system image, the build process expects the following
firmware and FPGA binaries to be present:

```
firmware/prod/piksi_firmware.elf
firmware/prod/piksi_fpga.bit
firmware/microzed/piksi_firmware.elf
firmware/microzed/piksi_fpga.bit
```

You can use the following command to download these binaries from S3. Note that
this requires `awscli` to be installed and AWS credentials to be properly
configured if you are building from a non-release branch.

``` sh
# Docker
make docker-make-firmware # (Requires 'make docker-setup` to be run first)

# Linux native
make firmware
```

Check `fetch-firmware.sh` to see which image versions are being used.

<u>Note:</u>
These binaries are only used when building a whole system image. In the
development system image they are instead read from the network or SD
card.

<u>Note:</u>
Only [tagged releases](https://github.com/swift-nav/piksi_buildroot/releases)
are made publicly available for download for use in building from source. Running
`make firmware` on the last tagged release in relation to an untagged branch or
commit may not always produce a functionial build, and therefore is not supported.

## Copying data out of docker

For speed, most buildroot data is captured inside a docker volume.  The volume
keeps file access inside a docker container instead of moving the data from
container to host (which is slow on many platforms, except Linux).

The following diagram shows how Docker composes the file system layers that we
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
         │        │  Docker volume:              │   │         docker.
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
