################################################################################
#
# starling_daemon
#
################################################################################

ifeq    ($(BR2_BUILD_STARLING_DAEMON),y)

STARLING_DAEMON_VERSION = v0.1.1
STARLING_DAEMON_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/starling_daemon"
STARLING_DAEMON_SITE_METHOD = local
STARLING_DAEMON_DEPENDENCIES = libuv libsbp libpiksi

# KEVIN NEEDS HELP
define STARLING_DAEMON_INSTALL_TARGET_CMDS
	$(INSTALL) *.so /usr/lib/
	$(INSTALL) piksi-multi-linux-starling /usr/bin
endef

# KEVIN NEEDS HELP
define STARLING_DAEMON_BUILD_CMDS
	${STARLING_DAEMON_SITE}/fetch_and_extract_tarball_from_github_releases.sh
endef

BR2_ROOTFS_OVERLAY += "${STARLING_DAEMON_SITE}/overlay"

$(eval $(generic-package))

endif # ($(BR2_BUILD_STARLING_DAEMON),y)
