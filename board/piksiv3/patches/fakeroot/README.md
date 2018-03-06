Fakeroot does not work inside of Linux namespaces which nix-shell uses for
FHS environments... this patch fixes that.

See https://github.com/NixOS/nixpkgs/issues/10496
