FROM debian:jessie

RUN echo 'deb http://ftp.debian.org/debian jessie-backports main' \
  >> /etc/apt/sources.list
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
  lib32z1
RUN apt-get -y --force-yes -t jessie-backports install \
  cmake

ENV BR2_EXTERNAL /piksi_buildroot
WORKDIR /piksi_buildroot
