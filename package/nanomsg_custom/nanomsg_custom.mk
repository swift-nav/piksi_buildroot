################################################################################
#
## nanomsg
#
#################################################################################

NANOMSG_CUSTOM_VERSION = 1.1.4
NANOMSG_CUSTOM_SITE = $(call github,nanomsg,nanomsg,$(NANOMSG_CUSTOM_VERSION))
NANOMSG_CUSTOM_INSTALL_STAGING = YES
NANOMSG_CUSTOM_LICENSE = MIT
NANOMSG_CUSTOM_LICENSE_FILES = COPYING
NANOMSG_CUSTOM_CONF_OPTS = -DNN_ENABLE_DOC=OFF -DNN_TESTS=OFF \
	-DCMAKE_C_FLAGS="$(HOST_CFLAGS) -O3" \
	-DCMAKE_CXX_FLAGS="$(HOST_CXXFLAGS) -O3" \
	-DCMAKE_VERBOSE_MAKEFILE=1

ifeq ($(BR2_STATIC_LIBS),y)
	NANOMSG_CUSTOM_CONF_OPTS += -DNN_STATIC_LIB=ON
endif

ifeq ($(BR2_PACKAGE_NANOMSG_CUSTOM_TOOLS),y)
	NANOMSG_CUSTOM_CONF_OPTS += -DNN_TOOLS=ON
else
	NANOMSG_CUSTOM_CONF_OPTS += -DNN_TOOLS=OFF
endif

$(eval $(cmake-package))
