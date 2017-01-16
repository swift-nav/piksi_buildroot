# Piksi Multi Buildroot

[![Build Status](https://travis-ci.org/swift-nav/piksi_buildroot.svg?branch=master)](https://travis-ci.org/swift-nav/piksi_buildroot)

[Buildroot](https://buildroot.org/) configuration for building the Piksi Multi
Linux system image.

## Fetching firmware images

To build a production system image, the build process expects the following
firmware and FPGA images to be present:

```
firmware/prod/piksi_firmware.elf
firmware/prod/piksi_fpga.bit
```

You can use the following script to download these images from S3. Note that
this script requires `awscli` to be installed and AWS credentials to be
properly configured.

``` sh
./fetch-firmware.sh
```

Check `fetch-firmware.sh` to see which image versions are being used.

Note, these firmware files are only used by the production system image. In the
development system image these files are instead read from the network or SD
card.

## Building

### Linux Native

Ensure you have the dependencies, see `Dockerfile` for build dependencies.

Run

``` sh
./build.sh
```

### Mac OS X

Native OS X builds are not supported by Buildroot. Install
[Docker for Mac](https://docs.docker.com/engine/installation/mac/) and then
follow the directions for installing with Docker.

### Docker

Builds in a Linux container.

``` sh
docker build -t piksi-buildroot .
```

To see the build output, tail the log file from inside Docker. First find the
container ID by running:

```
docker ps
```

Then tail the log (replace `1a77fcce79ed` with your container ID):

```
docker exec -i -t 1a77fcce79ed tail -f /app/build.out
```

To copy the built images out of the Docker container:

``` sh
mkdir -p buildroot/output
export CONTAINER=`docker create piksi-buildroot`
docker cp $CONTAINER:/app/buildroot/output/images buildroot/output
```

The buildroot images will now be in the `buildroot/output/images` folder.

