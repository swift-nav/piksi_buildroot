#!/bin/sh

/bin/hostname -F /etc/hostname

# now run any rc scripts
/etc/init.d/rcS

# clean-up the overlay directory
rm -r /stage1
rm -r /stage2
rm -r /overlay
rm -r /merged
rm -r /stage2.squashfs
