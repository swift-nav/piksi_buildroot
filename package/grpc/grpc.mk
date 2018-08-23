################################################################################
#
# grpc
#
################################################################################

GRPC_VERSION = v1.11.1
GRPC_SITE = $(call github,grpc,grpc,$(GRPC_VERSION))
GRPC_LICENSE = BSD-3-Clause
GRPC_LICENSE_FILES = LICENSE

# Need a host protoc grpc plugin during the compilation
GRPC_DEPENDENCIES = host-grpc gflags gtest c-ares openssl protobuf zlib
HOST_GRPC_DEPENDENCIES = host-c-ares host-protobuf host-openssl

GRPC_INSTALL_STAGING = YES

GRPC_CROSS_MAKE_OPTS_BASE = \
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
	$(GRPC_CROSS_MAKE_OPTS_BASE) \
	PROTOC="$(HOST_DIR)/usr/bin/protoc"

GRPC_INSTALL_TARGET_OPTS = \
	$(GRPC_CROSS_MAKE_OPTS_BASE) \
	prefix="$(TARGET_DIR)/usr"

GRPC_INSTALL_STAGING_OPTS = \
	$(GRPC_CROSS_MAKE_OPTS_BASE) \
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
	$(TARGET_MAKE_ENV) $(MAKE) $(GRPC_MAKE_OPTS) -C $(@D) \
		$(GRPC_BUILD_TARGETS)
endef

define GRPC_INSTALL_STAGING_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) $(GRPC_INSTALL_STAGING_OPTS) -C $(@D) \
		$(GRPC_STAGING_TARGETS)
endef

define GRPC_INSTALL_TARGET_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) $(GRPC_INSTALL_TARGET_OPTS) -C $(@D) \
		$(GRPC_INSTALL_TARGETS)
endef

HOST_GRPC_MAKE_OPTS = \
	CC="$(HOSTCC)" \
	CXX="$(HOSTCXX)" \
	LD="$(HOSTCC)" \
	LDXX="$(HOSTCXX)" \
	CPPLAGS="$(HOST_CPPFLAGS)" \
	CFLAGS="$(HOST_CFLAGS)" \
	CXXLAGS="$(HOST_CXXFLAGS)" \
	LDFLAGS="$(HOST_LDFLAGS)" \
	STRIP=/bin/true \
	PROTOC="$(HOST_DIR)/usr/bin/protoc" \
	prefix="$(HOST_DIR)"

define HOST_GRPC_BUILD_CMDS
	$(HOST_MAKE_ENV) $(MAKE) $(HOST_GRPC_MAKE_OPTS) -C $(@D) \
		plugins
endef

define HOST_GRPC_INSTALL_CMDS
	$(HOST_MAKE_ENV) $(MAKE) $(HOST_GRPC_MAKE_OPTS) -C $(@D) \
		install-plugins
endef

$(eval $(generic-package))
$(eval $(host-generic-package))
