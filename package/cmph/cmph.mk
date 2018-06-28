################################################################################
#
# cmph - http://cmph.sourceforge.net/
#
################################################################################

CMPH_VERSION = v2014.06.06
CMPH_SITE = $(call github,swift-nav,cmph-mirror,$(CMPH_VERSION))
CMPH_INSTALL_STAGING = YES
CMPH_LICENSE = LGPLv2
CMPH_LICENSE_FILES = LGPL-2
CMPH_AUTORECONF = YES
CMPH_AUTORECONF_OPTS = -i -f

define CMPH_POST_INSTALL_TARGET_FIXUP
	rm -v $(TARGET_DIR)/usr/lib/libcmph.la
	rm -v $(TARGET_DIR)/usr/bin/cmph
endef

CMPH_POST_INSTALL_TARGET_HOOKS += CMPH_POST_INSTALL_TARGET_FIXUP

$(eval $(autotools-package))
