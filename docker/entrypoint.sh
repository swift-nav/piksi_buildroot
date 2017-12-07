#!/bin/sh

export USER
export GID

sudo rm -rf "/home/$USER/.ssh"
sudo cp -r /host-ssh "/home/$USER/.ssh"

sudo chmod 0770 /root
sudo find /root -type f -exec chmod g+rw {} \;

sudo chown -R "$USER:$GID" "/home/$USER/.ssh"
sudo chmod 0700 "/home/$USER/.ssh"

sudo find "/home/$USER/.ssh" -type f -exec chmod 0400 {} \;

[ -d "/piksi_buildroot/buildroot" ] && \
  sudo chown "$USER:$GID" "/piksi_buildroot/buildroot"

[ -d "/piksi_buildroot/buildroot/output" ] && \
  sudo chown "$USER:$GID" "/piksi_buildroot/buildroot/output"

[ -d "/piksi_buildroot/buildroot/output/images" ] && \
  sudo chown "$USER:$GID" "/piksi_buildroot/buildroot/output/images"

exec sudo --preserve-env --user="$USER" --shell -- "$@"
