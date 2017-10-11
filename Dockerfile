FROM debian:stretch

RUN apt-get update
RUN apt-get -y --force-yes install \
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
  libc6-i386 \
  lib32stdc++6 \
  lib32z1 \
  libstdc++6 \
  vim \
  silversearcher-ag

ENV BR2_EXTERNAL /piksi_buildroot
WORKDIR /piksi_buildroot
