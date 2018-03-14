#!/bin/sh

/bin/hostname -F /etc/hostname

# now run any rc scripts
/etc/init.d/rcS

rm -r /stage1
rm -r /stage2
rm -r /overlay
rm -r /stage2.squashfs
