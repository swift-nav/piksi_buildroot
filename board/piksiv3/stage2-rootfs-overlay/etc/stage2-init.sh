#!/bin/sh

/bin/hostname -F /etc/hostname

# now run any rc scripts
/etc/init.d/rcS
