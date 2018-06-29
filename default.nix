{ pkgs ? import <nixpkgs> {} }:
 
let 
  ncurses5 = pkgs.ncurses.override { abiVersion = "5"; };
  fhs = pkgs.buildFHSUserEnv {
  name = "piksi-env";
  targetPkgs = pkgs: with pkgs; [
    awscli
    bash
    binutils
    bc
    bison
    bzip2
    cacert
    ccache
    cmake
    coreutils
    cpio
    curl
    db
    file
    flex
    gcc6
    gcc-arm-embedded
    git
    glibc
    glibc.dev
    gnugrep
    gnum4
    gnumake
    gnused
    gnutar
    gdb
    mercurial
    ncurses5
    ncurses5.dev
    nettools
    openssl
    openssl.dev
    patch
    patchelf
    perl
    python3
    python27Full
    readline
    readline.dev
    rsync
    sqlite
    sqlite.dev
    unzip
    utillinux
    valgrind
    vim
    wget
    which
    xz
    zlib
    valgrind
  ];
  multiPkgs = pkgs: with pkgs; [ ];
  runScript = "$SHELL";

  # We need to disable the gcc hardening that's enabled by default in the gcc
  # wrapper for NixOS, otherwise it'll cause packages to fail to compile in the
  # nix-shell environment.
  #
  # References:
  # - https://szibele.com/developing-software-on-nixos/
  # - https://github.com/NixOS/nixpkgs/issues/18995
  #
  # So we add hardeningDisable=all to the shell environment or we add it as an
  # argument to the mkDerivation call (however, since buildFHSUserEnv doesn't
  # understand this argument, it's added to the bash environement).
  profile = ''

    # buildFHSUserEnv seems to export NIX_CFLAGS_COMPILE and NIX_LDFLAGS_BEFORE
    #   but the cc-wrapper wants a var that includes a marker for the build
    #   triplet that it's wrapping:
    #
    export NIX_x86_64_unknown_linux_gnu_CFLAGS_COMPILE=$NIX_CFLAGS_COMPILE
    export NIX_x86_64_unknown_linux_gnu_LDFLAGS_BEFORE=$NIX_LDFLAGS_BEFORE 

    # Make sure buildroot Python loads dynamic modules from the right place
    export LD_LIBRARY_PATH=$PWD/buildroot/output/host/usr/lib:/lib:/usr/lib
    export PATH=$PWD/scripts/wrappers/bin:$PWD/buildroot/output/host/bin:$PATH

    # See note about hardeningDisable above
    export hardeningDisable=all

    # This is intended to be picked up by PS1 to mark that this is a subshell
    export debian_chroot='piksi-nix'

    # Workaround issus with SSL cert store not being propegrated for some
    # reason, per:
    #
    #   - https://github.com/NixOS/nixpkgs/issues/8534
    #   - https://github.com/NixOS/nixpkgs/issues/3382
    #
    export GIT_SSL_CAINFO=/etc/ssl/certs/ca-certificates.crt
    export CURL_CA_BUNDLE=/etc/ssl/certs/ca-certificates.crt
    export SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt

    export IN_PIKSI_SHELL=1
  '';
  
  extraBuildCommands = ''
  '';
};

in pkgs.stdenv.mkDerivation rec {
  name = "piksi-buildroot-env";
  nativeBuildInputs = [ fhs ];
  shellHook = ''
    if [ -z "$PS1" ]; then
      :
    else
      exec piksi-env
    fi
  '';
}
