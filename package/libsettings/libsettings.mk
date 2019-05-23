################################################################################
#
# libsettings
#
################################################################################

LIBSETTINGS_VERSION = woodfell/import_standard_cmake_modules
LIBSETTINGS_SITE = https://github.com/swift-nav/libsettings
LIBSETTINGS_SITE_METHOD = git
LIBSETTINGS_GIT_SUBMODULES = YES
LIBSETTINGS_INSTALL_STAGING = YES
LIBSETTINGS_CONF_OPTS = -DSWIFT_PREFERRED_DEPENDENCY_SOURCE=system
LIBSETTINGS_DEPENDENCIES = libsbp libswiftnav

define LIBSETTINGS_BUILD_CMDS
    $(MAKE) -C $(@D) settings
endef

$(eval $(cmake-package))
