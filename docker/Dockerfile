ARG VERSION_TAG

FROM swiftnav/buildroot-base:$VERSION_TAG

ARG USER
ARG UID
ARG GID

RUN groupadd --gid $GID $USER && \
    \
    useradd \
      --shell /bin/bash \
      --gid $USER \
      --groups sudo,root,tty \
      --uid "$UID" "$USER" && \
    \
    echo "$USER ALL=NOPASSWD: ALL" >/etc/sudoers.d/"$USER" && \
    \
    mkdir -p /home/"$USER"

ADD docker/entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
