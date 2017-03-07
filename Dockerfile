FROM debian:jessie

WORKDIR /app

ENV BR2_EXTERNAL=/app

RUN apt-get update && apt-get -y --force-yes install \
  build-essential \
  git \
  wget \
  python \
  unzip \
  bc \
  cpio

COPY . /app
#RUN make -C buildroot piksiv3_defconfig
#RUN HW_CONFIG=prod make -C buildroot 2>&1 | tee -a build.out | grep --line-buffered '^make'
#RUN HW_CONFIG=microzed make -C buildroot 2>&1 | tee -a build.out | grep --line-buffered '^make'