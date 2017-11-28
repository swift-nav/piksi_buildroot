#!/bin/sh

export USER=$USER
export UID=$UID

useradd \
  --shell /bin/bash \
  --gid users \
  --groups sudo,root,tty \
  --uid $UID $USER

echo "$USER ALL=NOPASSWD: ALL" >/etc/sudoers.d/$USER

mkdir -p /home/$USER

rm -rf /home/$USER/.ssh
cp -r /host-ssh /home/$USER/.ssh

chmod 0750 /root
find /root -type f -exec chmod g+rw {} \;

chown -R $USER:users /home/$USER/.ssh

chmod 0700 /home/$USER/.ssh
find /home/$USER/.ssh -type f -exec chmod 0400 {} \;

sudo --preserve-env --user=$USER --shell -- $*
