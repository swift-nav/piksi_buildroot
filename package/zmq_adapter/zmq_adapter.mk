################################################################################
#
# zmq_adapter
#
################################################################################

ZMQ_ADAPTER_VERSION = 0.1
ZMQ_ADAPTER_SITE = "${BR2_EXTERNAL}/package/zmq_adapter/src"
ZMQ_ADAPTER_SITE_METHOD = local
ZMQ_ADAPTER_DEPENDENCIES = czmq libsbp

define ZMQ_ADAPTER_BUILD_CMDS
    $(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define ZMQ_ADAPTER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/zmq_adapter $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
