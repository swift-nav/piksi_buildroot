################################################################################
#
# zmq_router
#
################################################################################

ZMQ_ROUTER_VERSION = 0.1
ZMQ_ROUTER_SITE = "${BR2_EXTERNAL}/package/zmq_router/src"
ZMQ_ROUTER_SITE_METHOD = local
ZMQ_ROUTER_DEPENDENCIES = czmq

define ZMQ_ROUTER_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define ZMQ_ROUTER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/zmq_router $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
