with import <nixpkgs> {};

stdenv.mkDerivation rec {
  name = "env";
  env = buildEnv { name = name; paths = buildInputs; ignoreCollisions = true; };
  buildInputs = [
    gnumake
    gcc6
    bash
    file
    flex
    bison
    cmake
    git
    mercurial
    unzip
    bc
    openssl.dev
    xz
    python27Full
    zlib
    bzip2
    db
    readline
    sqlite
    wget
    curl
    python
    ccache
  ];
  hardeningDisable = [ "all" ];
}
