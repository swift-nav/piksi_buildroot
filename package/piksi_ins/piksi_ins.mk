#############################################################
#
# piksi_ins
#
#############################################################

ifeq    ($(BR2_HAS_PIKSI_INS),y)
ifeq    ($(BR2_BUILD_RELEASE_PROTECTED),y)

$(info *** Piksi INS is enabled, packaging with current image...)

PIKSI_INS_VERSION = master
PIKSI_INS_SITE = git@github.com:carnegieroboticsllc/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = libsbp libpiksi eigen

$(eval $(cmake-package))

else

$(info *** Piksi INS was enabled, but image is not protected, ignoring...)

endif # ($(BR2_BUILD_RELEASE_PROTECTED),y)
endif # ($(BR2_HAS_PIKSI_INS),y)
