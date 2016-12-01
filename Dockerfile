FROM ubuntu:trusty

WORKDIR /app

RUN apt-get update && apt-get -y --force-yes install \
  build-essential \
  git \
  wget \
  python \
  unzip \
  bc \
  libssl-dev

COPY . /app

RUN /app/build.sh

