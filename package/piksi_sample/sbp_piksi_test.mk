PIKSI_SAMPLE_VERSION = 0.1
PIKSI_SAMPLE_SITE = "${BR2_EXTERNAL}/package/piksi_sample/src"
PIKSI_SAMPLE_DEPENDENCIES = czmq libsbp libpiksi
PIKSI_SAMPLE_SITE_METHOD = local

define PIKSI_SAMPLE_BUILD_CMDS
	$(MAKE) CC=$(TARGET_CC) LD=$(TARGET_LD) -C $(@D) all
endef

define PIKSI_SAMPLE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/piksi_sample $(TARGET_DIR)/usr/bin
endef

$(eval $(generic-package))
