################################################################################
#
# libsbp_zmq
#
################################################################################

LIBSBP_ZMQ_VERSION = 0.1
LIBSBP_ZMQ_SITE = "${BR2_EXTERNAL}/package/libsbp_zmq/src"
LIBSBP_ZMQ_SITE_METHOD = local
LIBSBP_ZMQ_DEPENDENCIES = czmq libsbp
LIBSBP_ZMQ_INSTALL_STAGING = YES

define LIBSBP_ZMQ_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define LIBSBP_ZMQ_INSTALL_STAGING_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libsbp_zmq.so* $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0755 $(@D)/libsbp_zmq.a $(STAGING_DIR)/usr/lib
    $(INSTALL) -D -m 0644 $(@D)/sbp_zmq.h $(STAGING_DIR)/usr/include
endef

define LIBSBP_ZMQ_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/libsbp_zmq.so* $(TARGET_DIR)/usr/lib
endef

$(eval $(generic-package))
