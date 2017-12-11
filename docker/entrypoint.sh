#!/bin/sh

export USER
export GID

## Fix-up ssh

sudo rm -rf "/home/$USER/.ssh"
sudo cp -r /host-ssh "/home/$USER/.ssh"

sudo chown -R "$USER:$GID" "/home/$USER/.ssh"
sudo chmod 0700 "/home/$USER/.ssh"

sudo find "/home/$USER/.ssh" -type f -exec chmod 0400 {} \;

## Fix-up aws

sudo rm -rf "/home/$USER/.aws"
sudo cp -r /host-aws "/home/$USER/.aws"

sudo chown -R "$USER:$GID" "/home/$USER/.aws"
sudo chmod 0700 "/home/$USER/.aws"

sudo find "/home/$USER/.aws" -type f -exec chmod 0400 {} \;

## Fix-up root home dir

sudo chmod 0770 /root
sudo find /root -type f -exec chmod g+rw {} \;

[ -d "/piksi_buildroot/buildroot" ] && \
  sudo chown "$USER:$GID" "/piksi_buildroot/buildroot"

[ -d "/piksi_buildroot/buildroot/output" ] && \
  sudo chown "$USER:$GID" "/piksi_buildroot/buildroot/output"

[ -d "/piksi_buildroot/buildroot/output/images" ] && \
  sudo chown "$USER:$GID" "/piksi_buildroot/buildroot/output/images"

exec sudo --preserve-env --user="$USER" --shell -- "$@"
