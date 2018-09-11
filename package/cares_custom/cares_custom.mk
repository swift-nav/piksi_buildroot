################################################################################
#
# c-ares
#
################################################################################

CARES_CUSTOM_VERSION = 1.13.0
CARES_CUSTOM_SOURCE = c-ares-$(CARES_CUSTOM_VERSION).tar.gz
CARES_CUSTOM_SITE = https://c-ares.haxx.se/download
CARES_CUSTOM_INSTALL_STAGING = YES
CARES_CUSTOM_CONF_OPTS = --with-random=/dev/urandom
# Rebuild configure to avoid XC_CHECK_USER_CFLAGS
CARES_CUSTOM_AUTORECONF = YES
CARES_CUSTOM_LICENSE = MIT
# No standalone, use some source file
CARES_CUSTOM_LICENSE_FILES = ares_mkquery.c

$(eval $(host-autotools-package))
