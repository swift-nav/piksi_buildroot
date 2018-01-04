#!/bin/bash

export USER
export GID

## Fix-up ssh

sudo rm -rf "/home/$USER/.ssh"
sudo cp -r /host/home/.ssh "/home/$USER/.ssh"

sudo chown -R "$USER:$GID" "/home/$USER/.ssh"
sudo chmod 0700 "/home/$USER/.ssh"

sudo find "/home/$USER/.ssh" -type f -exec chmod 0400 {} \;

## Fix-up aws

sudo rm -rf "/home/$USER/.aws"
sudo cp -r /host/home/.aws "/home/$USER/.aws"

sudo chown -R "$USER:$GID" "/home/$USER/.aws"
sudo chmod 0700 "/home/$USER/.aws"

sudo find "/home/$USER/.aws" -type f -exec chmod 0400 {} \;

## Fix-up root home dir

sudo chmod 0770 /root
sudo find /root -type f -exec chmod g+rw {} \;

## Copy in persistent history

[ -e "/host/tmp/piksi_buildroot_bash_history" ] && \
  { sudo cp /host/tmp/piksi_buildroot_bash_history /home/$USER/.bash_history;
    sudo chown $USER:$GID /home/$USER/.bash_history;
  }

[ -d "/piksi_buildroot/buildroot" ] && \
  sudo chown "$USER:$GID" "/piksi_buildroot/buildroot"

[ -d "/piksi_buildroot/buildroot/output" ] && \
  sudo chown "$USER:$GID" "/piksi_buildroot/buildroot/output"

[ -d "/piksi_buildroot/buildroot/output/images" ] && \
  sudo chown "$USER:$GID" "/piksi_buildroot/buildroot/output/images"

sync_in() {
  if [[ -z "$ONLY_SYNC_OUT" ]]; then

    echo "Syncing files into docker..."
    trap 'echo Cannot cancel file sync\!' SIGINT

    sudo rsync \
      --exclude=.git --exclude=buildroot/output/images \
      --archive --update --delete-during --ignore-errors \
      --no-owner --no-group --numeric-ids \
      /host/piksi_buildroot/ /piksi_buildroot/

    trap - SIGINT
  fi
}

sync_out() {
  echo "Syncing files out of docker..."

  trap 'echo Cannot cancel file sync\!' SIGINT

  sudo rsync \
    --exclude=.git --exclude=buildroot/output/images \
    --archive --update --delete-during --ignore-errors \
    --no-owner --no-group --numeric-ids  \
    /piksi_buildroot/ /host/piksi_buildroot/

  trap - SIGINT

  [ -e "/home/$USER/.bash_history" ] \
    && sudo cp /home/$USER/.bash_history /host/tmp/piksi_buildroot_bash_history \
    || true
}

sync_in

sudo --preserve-env --user="$USER" --shell -- "$@" || true

sync_out

# vim: ff=unix:
