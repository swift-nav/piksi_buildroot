################################################################################
#
# grpc
#
################################################################################

GRPC_CUSTOM_VERSION = v1.12.0
GRPC_CUSTOM_SITE = https://github.com/grpc/grpc.git
GRPC_CUSTOM_SITE_METHOD = git
GRPC_CUSTOM_LICENSE = BSD-3-Clause
GRPC_CUSTOM_LICENSE_FILES = LICENSE

GRPC_CUSTOM_DEPENDENCIES = gflags gtest cares_custom openssl protobuf_custom zlib

GRPC_CUSTOM_INSTALL_STAGING = YES

GRPC_CUSTOM_MAKE_ENV = \
	$(TARGET_MAKE_ENV) \
	GRPC_CROSS_COMPILE="true" \
	GRPC_CROSS_LDOPTS="-L$(HOST_DIR)/usr/lib" \
	LDCONFIG=/bin/true \
	CC="$(TARGET_CC)" \
	CXX="$(TARGET_CXX)" \
	LD="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	STRIP=/bin/true \
  V=1

GRPC_CUSTOM_MAKE_OPTS = \
	PROTOC="$(HOST_DIR)/usr/bin/protoc"

GRPC_CUSTOM_INSTALL_TARGET_OPTS = \
	prefix="$(TARGET_DIR)/usr"

GRPC_CUSTOM_INSTALL_STAGING_OPTS = \
	prefix="$(STAGING_DIR)/usr"

GRPC_CUSTOM_BUILD_TARGETS = static
GRPC_CUSTOM_STAGING_TARGETS = install-headers install-static_c install-static_cxx
GRPC_CUSTOM_INSTALL_TARGETS = install-static_c install-static_cxx

define GRPC_CUSTOM_BUILD_CMDS
	$(GRPC_CUSTOM_MAKE_ENV) $(MAKE) $(GRPC_CUSTOM_MAKE_OPTS) -C $(@D) \
		$(GRPC_CUSTOM_BUILD_TARGETS)
endef

define GRPC_CUSTOM_INSTALL_STAGING_CMDS
	$(GRPC_CUSTOM_MAKE_ENV) $(MAKE) $(GRPC_CUSTOM_INSTALL_STAGING_OPTS) -C $(@D) \
		$(GRPC_CUSTOM_STAGING_TARGETS)
endef

define GRPC_CUSTOM_INSTALL_TARGET_CMDS
	$(GRPC_CUSTOM_MAKE_ENV) $(MAKE) $(GRPC_CUSTOM_INSTALL_TARGET_OPTS) -C $(@D) \
		$(GRPC_CUSTOM_INSTALL_TARGETS)
endef

$(eval $(generic-package))
