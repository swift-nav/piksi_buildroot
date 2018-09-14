################################################################################
#
# protobuf
#
################################################################################

# When bumping this package, make sure to also verify if the
# python-protobuf package still works, as they share the same
# version/site variables.
PROTOBUF_CUSTOM_VERSION = 3.6.0
PROTOBUF_CUSTOM_SOURCE = protobuf-cpp-$(PROTOBUF_CUSTOM_VERSION).tar.gz
PROTOBUF_CUSTOM_SITE = https://github.com/google/protobuf/releases/download/v$(PROTOBUF_CUSTOM_VERSION)
PROTOBUF_CUSTOM_LICENSE = BSD-3-Clause
PROTOBUF_CUSTOM_LICENSE_FILES = LICENSE

# N.B. Need to use host protoc during cross compilation.
PROTOBUF_CUSTOM_DEPENDENCIES = host-protobuf_custom
PROTOBUF_CUSTOM_CONF_OPTS = --with-protoc=$(HOST_DIR)/bin/protoc

ifeq ($(BR2_TOOLCHAIN_HAS_LIBATOMIC),y)
PROTOBUF_CUSTOM_CONF_ENV += LIBS=-latomic
endif

PROTOBUF_CUSTOM_INSTALL_STAGING = YES

ifeq ($(BR2_PACKAGE_ZLIB),y)
PROTOBUF_CUSTOM_DEPENDENCIES += zlib
endif

$(eval $(autotools-package))
$(eval $(host-autotools-package))
