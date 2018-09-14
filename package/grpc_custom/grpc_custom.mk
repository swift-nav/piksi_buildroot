################################################################################
#
# grpc_custom
#
################################################################################

GRPC_CUSTOM_VERSION = v1.12.0
GRPC_CUSTOM_SITE = https://github.com/grpc/grpc.git
GRPC_CUSTOM_SITE_METHOD = git
GRPC_CUSTOM_LICENSE = BSD-3-Clause
GRPC_CUSTOM_LICENSE_FILES = LICENSE

GRPC_CUSTOM_DEPENDENCIES = host-grpc_custom gflags gtest c-ares openssl protobuf_custom zlib
HOST_GRPC_CUSTOM_DEPENDENCIES = host-cares_custom host-protobuf_custom host-openssl

GRPC_CUSTOM_INSTALL_STAGING = YES

GRPC_CUSTOM_MAKE_ENV = \
	CC="$(TARGET_CC)" \
	CXX="$(TARGET_CXX)" \
	LD="$(TARGET_CC)" \
	LDXX="$(TARGET_CXX)" \
	STRIP="$(TARGET_STRIP)" \
	PROTOC="$(HOST_DIR)/bin/protoc" \
	PATH="$(HOST_DIR)/bin:$(PATH)" \
	GRPC_CROSS_COMPILE=true \
	GRPC_CROSS_LDOPTS="$(TARGET_LDFLAGS)" \
	GRPC_CROSS_AROPTS="$(LTO_PLUGIN)" \
	HAS_PKG_CONFIG=false \
	PROTOBUF_CONFIG_OPTS="--host=$(GNU_TARGET_NAME) --with-protoc=$(HOST_DIR)/bin/protoc" \
	HOST_CC="$(HOSTCC)" \
	HOST_CXX="$(HOSTCXX)" \
	HOST_LD="$(HOSTCC)" \
	HOST_LDXX="$(HOSTCXX)" \
	HOST_CPPFLAGS="$(HOST_CPPFLAGS)" \
	HOST_CFLAGS="$(HOST_CFLAGS)" \
	HOST_CXXFLAGS="$(HOST_CXXFLAGS)" \
	HOST_LDFLAGS="$(HOST_LDFLAGS)"

GRPC_CUSTOM_MAKE_OPTS = \
	LD_LIBRARY_PATH="$(STAGING_DIR)/usr/lib" \
	PROTOC="$(HOST_DIR)/bin/protoc"

GRPC_CUSTOM_INSTALL_TARGET_OPTS = \
	prefix="$(TARGET_DIR)/usr"

GRPC_CUSTOM_INSTALL_STAGING_OPTS = \
	prefix="$(STAGING_DIR)/usr"

define GRPC_CUSTOM_BUILD_CMDS
	$(GRPC_CUSTOM_MAKE_ENV) $(MAKE) $(GRPC_CUSTOM_MAKE_OPTS) -C $(@D) \
		shared_c shared_cxx
endef

define GRPC_CUSTOM_INSTALL_STAGING_CMDS
	$(GRPC_CUSTOM_MAKE_ENV) $(MAKE) $(GRPC_CUSTOM_INSTALL_STAGING_OPTS) -C $(@D) \
		install-headers install-shared_c install-shared_cxx
endef

define GRPC_CUSTOM_INSTALL_TARGET_CMDS
	$(GRPC_CUSTOM_MAKE_ENV) $(MAKE) $(GRPC_CUSTOM_INSTALL_TARGET_OPTS) -C $(@D) \
		install-shared_c install-shared_cxx
endef

HOST_GRPC_CUSTOM_MAKE_ENV = \
	$(HOST_MAKE_ENV) \
	CFLAGS="$(HOST_CFLAGS)" \
	LDFLAGS="$(HOST_LDFLAGS)"

HOST_GRPC_CUSTOM_MAKE_OPTS = \
	LD_LIBRARY_PATH="$(HOST_DIR)/usr/lib" \
	prefix="$(HOST_DIR)/usr"

define HOST_GRPC_CUSTOM_BUILD_CMDS
	$(HOST_GRPC_CUSTOM_MAKE_ENV) $(MAKE) $(HOST_GRPC_CUSTOM_MAKE_OPTS) -C $(@D) \
		plugins
endef

define HOST_GRPC_CUSTOM_INSTALL_CMDS
	$(HOST_GRPC_CUSTOM_MAKE_ENV) $(MAKE) $(HOST_GRPC_CUSTOM_MAKE_OPTS) -C $(@D) \
		install-plugins
endef

$(eval $(generic-package))
$(eval $(host-generic-package))
