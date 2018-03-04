# This is necessary since many projects in buildroot hardcode paths to certain
#  system tools.
#
# Ideally this should be invoked from nix-shell somehow, and undone when
#   nix-shell termintes.

sudo ln -s `which bash` /bin/
sudo ln -s `which file` /usr/bin/

## TODO: Figure out a way to discovery this
sudo mkdir /lib64
sudo ln -sf /nix/store/d54amiggq6bw23jw6mdsgamvs6v1g3bh-glibc-2.25-123/lib64/ld-linux-x86-64.so.2 /lib64/

sudo ln -sf $(which true) /bin/
sudo ln -sf $(which install) /usr/bin/
sudo ln -sf $(which pwd) /bin/
sudo ln -sf $(which mv) /usr/bin
