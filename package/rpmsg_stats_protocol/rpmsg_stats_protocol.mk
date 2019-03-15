################################################################################
#
# rpmsg_stats_protocol
#
################################################################################

RPMSG_STATS_PROTOCOL_VERSION = 0.1
RPMSG_STATS_PROTOCOL_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/rpmsg_stats_protocol/src"
RPMSG_STATS_PROTOCOL_SITE_METHOD = local
RPMSG_STATS_PROTOCOL_DEPENDENCIES =
RPMSG_STATS_PROTOCOL_INSTALL_STAGING = YES

define RPMSG_STATS_PROTOCOL_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) LTO_PLUGIN="$(LTO_PLUGIN)" -C $(@D) all
endef

define RPMSG_STATS_PROTOCOL_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/librpmsg_stats_protocol.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/librpmsg_stats_protocol.a $(STAGING_DIR)/usr/lib
endef

define RPMSG_STATS_PROTOCOL_INSTALL_TARGET_CMDS
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/lib/endpoint_protocols
    $(INSTALL) -D -m 0755 $(@D)/librpmsg_stats_protocol.so*                         \
                          $(TARGET_DIR)/usr/lib/endpoint_protocols
    $(INSTALL) -d -m 0755 $(TARGET_DIR)/etc/endpoint_router
    $(INSTALL) -D -m 0755 $(@D)/rpmsg_stats_router.yml $(TARGET_DIR)/etc/endpoint_router
endef

$(eval $(generic-package))
