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
this requires `awscli` to be installed and AWS credentials to be properly
configured.

```
# Docker
make docker-make-firmware # (Requires 'make docker-setup` to be run first)

# Linux native
make firmware
```

Check `fetch-firmware.sh` to see which image versions are being used.

Note that these binaries are only used by the production system image. In the
development system image they are instead read from the network or SD
card.

## Building

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

## Incremental Builds

It is possible to rebuild individual packages and regenerate the system image. Note that Buildroot does _not_ automatically rebuild dependencies or handle configuration changes. In some cases a full rebuild may be necessary. See the [Buildroot Manual](https://buildroot.org/downloads/manual/manual.html) for details.

### Docker

``` sh
# Start interactive Docker container
make docker-run
# Enter buildroot directory
cd buildroot
# Rebuild a package
make libpiksi-rebuild
# Rebuild the system image
make image
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
```
# Docker
make docker-make-image

# or, Linux native
make image
```
