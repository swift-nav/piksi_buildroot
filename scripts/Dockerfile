ARG VERSION_TAG

FROM swiftnav/buildroot-base:$VERSION_TAG

ARG USER
ARG UID
ARG GID

RUN ( grep -q :$GID: /etc/group || groupadd --gid $GID $USER ) && \
    useradd \
      --shell /bin/bash \
      --gid $GID \
      --groups sudo,root,tty \
      --uid "$UID" "$USER" && \
    echo "$USER ALL=NOPASSWD: ALL" >/etc/sudoers.d/"$USER" && \
    mkdir -p /home/"$USER" && \
    chown "$USER:$GID" /home/"$USER"

ADD scripts/docker_entrypoint.sh /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
