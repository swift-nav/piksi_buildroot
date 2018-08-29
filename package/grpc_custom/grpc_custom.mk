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

GRPC_CUSTOM_DEPENDENCIES = host-grpc_custom gflags gtest cares_custom openssl protobuf_custom zlib
HOST_GRPC_DEPENDENCIES = host-cares_custom host-protobuf host-openssl

GRPC_CUSTOM_INSTALL_STAGING = YES

GRPC_CUSTOM_MAKE_ENV = \
	$(TARGET_MAKE_ENV) \
	GRPC_CUSTOM_CROSS_COMPILE="true" \
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

GRPC_CUSTOM_MAKE_OPTS = \
	PROTOC="$(HOST_DIR)/usr/bin/protoc"

GRPC_CUSTOM_INSTALL_TARGET_OPTS = \
	prefix="$(TARGET_DIR)/usr"

GRPC_CUSTOM_INSTALL_STAGING_OPTS = \
	prefix="$(STAGING_DIR)/usr"

ifeq ($(BR2_SHARED_LIBS),y)
GRPC_CUSTOM_BUILD_TARGETS = shared plugins
GRPC_CUSTOM_STAGING_TARGETS = install-headers install-shared_c install-shared_cxx
GRPC_CUSTOM_INSTALL_TARGETS = install-shared_c install-shared_cxx
else ifeq ($(BR2_STATIC_LIBS),y)
GRPC_CUSTOM_BUILD_TARGETS = static plugins
GRPC_CUSTOM_STAGING_TARGETS = install-headers install-static_c install-static_cxx
GRPC_CUSTOM_INSTALL_TARGETS = install-static_c install-static_cxx
else
GRPC_CUSTOM_BUILD_TARGETS = static shared plugins
GRPC_CUSTOM_STAGING_TARGETS = install
GRPC_CUSTOM_INSTALL_TARGETS = install-shared_c install-shared_cxx
endif

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

HOST_GRPC_CUSTOM_MAKE_OPTS = \
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

define HOST_GRPC_CUSTOM_BUILD_CMDS
	$(HOST_MAKE_ENV) $(MAKE) $(HOST_GRPC_CUSTOM_MAKE_OPTS) -C $(@D) \
		plugins
endef

define HOST_GRPC_CUSTOM_STAGING_CMDS
	$(HOST_MAKE_ENV) $(MAKE) $(HOST_GRPC_CUSTOM_MAKE_OPTS) -C $(@D) \
		install
endef

define HOST_GRPC_CUSTOM_INSTALL_CMDS
	$(HOST_MAKE_ENV) $(MAKE) $(HOST_GRPC_CUSTOM_MAKE_OPTS) -C $(@D) \
		install-plugins
endef

$(eval $(generic-package))
$(eval $(host-generic-package))
