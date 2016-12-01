# Piksi Multi Buildroot

[![Build Status](https://travis-ci.org/swift-nav/piksi_buildroot.svg?branch=master)](https://travis-ci.org/swift-nav/piksi_buildroot)

[Buildroot](https://buildroot.org/) configuration for building the Piksi Multi
Linux system image.

## Building

### Linux Native

Ensure you have the dependencies, see `Dockerfile` for details.

Run

``` sh
./build.sh
```

### Docker

Builds in a Linux container.

``` sh
docker build -t piksi-buildroot .
```

To copy the built images out of the Docker container:

``` sh
mkdir -p buildroot/output
export CONTAINER=`docker create piksi-buildroot`
docker cp $CONTAINER:/app/buildroot/output/images buildroot/output
```

The buildroot images will now be in the `buildroot/output/images` folder.

### Mac OS X

Native OS X builds are not supported by Buildroot. Install
[Docker for Mac](https://docs.docker.com/engine/installation/mac/) and then
follow the directions for installing with Docker.

