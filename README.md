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

## Jenkins Builds

The pipeline defined in Jenkinsfile is using docker containers. To reproduce the same container as used
by Jenkins, run `make docker-jenkins` from the repo root dir; this will open a shell in the container.
The workspace is mounted to /mnt/workspace.

Note that on non-Linux hosts, the mounted filesystem's performance is very low, so a full build may take
a long time.

The alternative is to go to the users homedir, `git clone` the repo into the container, and run any build
on that container-native filesystem:
```bash
> make docker-jenkins
jenkins@9896b6d04be2:/mnt/workspace$ cd ~

jenkins@9896b6d04be2:~$ git clone ssh://git@github.com/swift-nav/piksi_buildroot
Cloning into 'piksi_buildroot'...

jenkins@9896b6d04be2:~$ cd piksi_buildroot/
jenkins@9896b6d04be2:~/piksi_buildroot$ git submodule update --init --recursive
[...]
jenkins@9896b6d04be2:~/piksi_buildroot$ make firmware image
```

## Development

### Formatting

When submitting pull requests, automated build infrastructure will apply a standardized formatting
rubric via `clang-format` to validate any style related changes that might be requested during review
(see [nits](https://stackoverflow.com/questions/27810522/what-does-nit-mean-in-hacker-speak)). This
is in an effort to keep review focused on the functionality of the changes, and not on nits.

Use the following target to format your changes before submitting (must have `clang-format==5.0` installed):

``` sh
# Docker
make docker-clang-format

# Linux native
make clang-format
```

### Build Variants and External Artifacts

Build variants are controlled by the `build-variants.yaml` config file which
specifies the inputs into a variant based on a number of inputs:

+ The name of the build variant and the package that should be generated
  - `variant_name`: Controls the name of variant, a variant is selected by
    settings the `VARIANT` environment variable (if unset, `internal` is the
    default).  For example, to build the `release` variant:
    ```
    make VARIANT=release image
    ```
  - `image_name`: controls the name of output package,
    for example `'PiksiMulti-INTERNAL-{version}.bin'`
  - `encrypted`: controls wether the output package should be encrypted
+ A number pre-flight checks to verify build inputs
  - `pre_flight`: a list of scripts to run to verify the build environment
     prior to running.  For example:
    ```yaml
     pre_flight:
       - run: './scripts/verify-aws-access'
       - run: './scripts/verify-generated-configs'
    ```
    Will run the scripts `./scripts/verify-aws-access` and
    `./scripts/verify-generated-configs` -- if any of the scripts fail, then
    the build will be terminated.
+ Input config fragments
  - `config_fragments`: A list of config fragments used to generate an output
    config, for example:
    ```yaml
     config_fragments:
       - path: configs/fragments/piksiv3/core
       - blob: 'BR2_PACKAGE_PIKSI_INS_REF=y'
    ```
  - `config_output`: specifies the output path of the resulting config, for
    example `'configs/piksiv3_internal_defconfig'`
+ An output directory: specified by the `output` config field, the build
  materials from buildroot will be placed here.
+ An external artifact set: specifies a set of external artifacts that are
  inputs into the build.  The `external-artifacts.yaml` config file specifies
  these artifacts.

Example config block:

```yaml
  -
   variant:
     variant_name: 'internal'
     encrypted: false
     artifact_set: 'internal'
     image_name:
       'PiksiMulti-INTERNAL-{version}.bin'
     output: 'output/internal'
     pre_flight:
       - run: './scripts/verify-aws-access'
       - run: './scripts/verify-piksi-ins-axx'
       - run: './scripts/verify-generated-configs'
     config_fragments:
       - path: configs/fragments/piksiv3/core
       - path: configs/fragments/piksiv3/core_packages
       - path: configs/fragments/piksiv3/piksi_ins
       - blob: 'BR2_PACKAGE_PIKSI_INS_REF=y'
       - blob: 'BR2_PACKAGE_SAMPLE_DAEMON=y'
       - blob: 'BR2_PACKAGE_STARLING_DAEMON=y'
       - blob: 'BR2_PACKAGE_PIKSI_DEV_TOOLS=y'
     config_output:
       'configs/piksiv3_internal_defconfig'
```

#### Working with `*_defconfig` files

##### Why not use kconfig options?

Using kconfig options is an alternative to implement our build variants --
(probably more obvious to those familiar with kconfig)-- this simple method of
composing fragments is more straightfoward to those not familiar with kconfig
and makes it painfully obbvious what's in each variant, available from one
top-level config file (at the expense of making it somewhat more tedious to
update configs).

##### How do I add a new package?

If you're adding a new package to the `package` dir.  Simply add the
`BR2_PACKAGE_MY_NEW_PACKAGE=y` to one of the config fragments in
`config/fragments`.  Typically this'll will be to
`configs/fragments/core_packages` so that the package is enabled for all
variants.  Currently the `nano` and `host` configs do not use fragments (or
rather they only use one fragment, and do not use the common
`configs/fragments/core_packages`), so the package will need to be manually
enabled for those variants in `configs/fragments/nano` and/or
`config/fragments/host`.  

##### How do I use kconfig to update the config?

Run menuconfig per usual:

```
make docker-shell # if running docker
make VARIANT=<variant-name> config
make -C buildroot menuconfig
# ... update config with menuconfig ...
make -C savedefconfig
```

Then use `git diff` to inspect the changes and move the necessary changes from
`configs/<variant>_defconfig` to one of the fragments in `configs/fragments`.
Use `make gen-variant-configs` (described in the next section) to update the
generated `*_defconfig` file.

##### Generating configs

An output config (specified in `config_output` field) can be generated by
running the following make commands:

```
make gen-variant-configs
```

In the general case, updates to configs should be placed in one of the files in
the `configs/fragments` directory and a new output config must be generated for
these config changes to be utilized.  If working with `make menuconfig`
(kconfig) to update a buildroot config, see the previous section.

#### External artifacts

The `external-artifacts.yaml` file controls what artifacts are pulled from S3 as
inputs into the build process.  These could be any number of things, but this is
primarily used to download the RTOS ELF image, FPGA bitstream, and any
pre-compiled ARM Linux binaries.

Explanation of config fields:
- `name`: a friendly name for the artifact set, used by `build-variants.yaml` to
  refer to particular artifact set.  Also used by the `gen-requirements-yaml`
  script to generate the `requirements.yaml` file that HITL consumes.
- `artifacts`: a list of artifacts that are part of the set

Explanation of fields for the entries in an `artifacts` list:
- `name`: a frieldly name of the artifact, used by the `get-named-artifact`
  script to locate a specific artifact
- `version`: the version of the specified artifact
- `s3_bucket`: the S3 bucket in which the artifact is located
- `s3_repository`: the name of the repository this artifact is sourced from, for example: `"piksi_firmware_private"`,
                   `"piksi_upgrade_tool"`, `"starling_daemon"`, etc.
- `s3_object`: the path to the actual object (file) within the repository
- `local_path`: where the file should be placed on the system
- `sha256`: a SHA256 hash of the file

All fields of the entries in the `artifacts` list are string template enabled.
Allowing for values such as `'{name}/{version}/piksiv3'`, where `{name}` refers
to the `name` field in the artifact list entry.

Example `artifact_set` config blob:

```yaml
- artifact_set:
    name: base
    artifacts:
    - name: rtos_elf
      version: v2.2.0-develop-2019040519
      s3_bucket: swiftnav-artifacts
      s3_repository: piksi_firmware_private
      s3_object: v3/piksi_firmware_v3_base.stripped.elf
      local_path: firmware/prod/piksi_firmware.elf
      sha256: 1694f19046ceafc7e4d35614c89e65c33a253adc84c910be120273c7a15e0034
    - name: fpga_bitstream
      version: v2.2.0-develop-2019040519
      s3_bucket: swiftnav-artifacts
      s3_repository: piksi_fpga
      s3_object: piksi_base_fpga.bit
      local_path: firmware/prod/piksi_fpga.bit
      sha256: 4b3450c649a94d4311e081a071a5877f316a1f4c1de7e3640b87cfac6545ce45

# ... elided for brevity ...
```
