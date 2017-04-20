FROM debian:jessie

RUN apt-get update && apt-get -y --force-yes install \
  build-essential \
  git \
  wget \
  python \
  unzip \
  bc \
  cpio \
  libssl-dev \
  ncurses-dev \
  mercurial \
  cmake \
  libc6-i386 \
  lib32stdc++6 \
  lib32z1

ENV BR2_EXTERNAL /piksi_buildroot
WORKDIR /piksi_buildroot
