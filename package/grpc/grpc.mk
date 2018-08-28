################################################################################
#
# grpc
#
################################################################################

GRPC_VERSION = v1.12.0
GRPC_SITE = https://github.com/grpc/grpc.git
GRPC_SITE_METHOD = git
GRPC_LICENSE = BSD-3-Clause
GRPC_LICENSE_FILES = LICENSE

GRPC_DEPENDENCIES = gflags gtest c-ares openssl protobuf_custom zlib

GRPC_INSTALL_STAGING = YES

GRPC_MAKE_ENV = \
	$(TARGET_MAKE_ENV) \
	GRPC_CROSS_COMPILE="true" \
	LDCONFIG=/bin/true \
	HOST_CC="$(HOSTCC)" \
	HOST_CXX="$(HOSTCXX)" \
	HOST_LD="$(HOSTCC)" \
	HOST_LDXX="$(HOSTCXX)" \
	HOST_CPPFLAGS="$(HOST_CPPFLAGS)" \
	HOST_CFLAGS="$(HOST_CFLAGS)" \
	HOST_LDFLAGS="$(HOST_LDFLAGS)" \
	CC="$(TARGET_CC)" \
	CXX="$(TARGET_CXX)" \
	LD="$(TARGET_CC)" \
	LDXX="$(TARGET_CXX)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	STRIP=/bin/true

GRPC_MAKE_OPTS = \
	PROTOC="$(HOST_DIR)/usr/bin/protoc"

GRPC_INSTALL_TARGET_OPTS = \
	prefix="$(TARGET_DIR)/usr"

GRPC_INSTALL_STAGING_OPTS = \
	prefix="$(STAGING_DIR)/usr"

ifeq ($(BR2_SHARED_LIBS),y)
GRPC_BUILD_TARGETS = shared plugins
GRPC_STAGING_TARGETS = install-headers install-shared_c install-shared_cxx
GRPC_INSTALL_TARGETS = install-shared_c install-shared_cxx
else ifeq ($(BR2_STATIC_LIBS),y)
GRPC_BUILD_TARGETS = static plugins
GRPC_STAGING_TARGETS = install-headers install-static_c install-static_cxx
GRPC_INSTALL_TARGETS = install-static_c install-static_cxx
else
GRPC_BUILD_TARGETS = static shared plugins
GRPC_STAGING_TARGETS = install
GRPC_INSTALL_TARGETS = install-shared_c install-shared_cxx
endif

define GRPC_BUILD_CMDS
	$(GRPC_MAKE_ENV) $(MAKE) $(GRPC_MAKE_OPTS) -C $(@D) \
		$(GRPC_BUILD_TARGETS)
endef

define GRPC_INSTALL_STAGING_CMDS
	$(GRPC_MAKE_ENV) $(MAKE) $(GRPC_INSTALL_STAGING_OPTS) -C $(@D) \
		$(GRPC_STAGING_TARGETS)
endef

define GRPC_INSTALL_TARGET_CMDS
	$(GRPC_MAKE_ENV) $(MAKE) $(GRPC_INSTALL_TARGET_OPTS) -C $(@D) \
		$(GRPC_INSTALL_TARGETS)
endef

$(eval $(generic-package))
$(eval $(host-generic-package))
