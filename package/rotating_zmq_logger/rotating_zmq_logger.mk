################################################################################
#
# rotating_zmq_logger
#
################################################################################

ROTATING_ZMQ_LOGGER_VERSION = 0.1
ROTATING_ZMQ_LOGGER_SITE = "${BR2_EXTERNAL}/package/rotating_zmq_logger/src"
ROTATING_ZMQ_LOGGER_SITE_METHOD = local
ROTATING_ZMQ_LOGGER_DEPENDENCIES = czmq

define ROTATING_ZMQ_LOGGER_BUILD_CMDS
    $(MAKE) CROSS=$(TARGET_CROSS) LD=$(TARGET_LD) -C $(@D) all
endef

define ROTATING_ZMQ_LOGGER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/rotating_zmq_logger $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
