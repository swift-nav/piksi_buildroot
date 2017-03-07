FROM debian:jessie

WORKDIR /app

RUN apt-get update && apt-get -y --force-yes install \
  build-essential \
  git \
  wget \
  python \
  unzip \
  bc \
  cpio \
  libssl-dev

COPY . /app

RUN HW_CONFIG=prod make image 2>&1 | tee -a build.out | grep --line-buffered '^make'
RUN HW_CONFIG=microzed make image 2>&1 | tee -a build.out | grep --line-buffered '^make'