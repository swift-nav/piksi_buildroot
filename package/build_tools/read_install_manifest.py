#!/usr/bin/env python

import sys

if len(sys.argv) < 2:
    sys.stderr.write("ERROR: need one command line argument\n")
    sys.stderr.write("{}: <install_manifest_path>\n")
    sys.stderr.flush()
    sys.exit(1)

install_manifest = sys.argv[1]
install_manifest = open(install_manifest, 'r')

for filename in install_manifest.read().split('\n'):
    print(filename)
