#############################################################
#
# piksi_ins
#
#############################################################

ifeq    ($(BR2_HAS_PIKSI_INS),y)
ifneq   ($(BR2_BUILD_RELEASE_PROTECTED),y)

$(info >>> *******************************************************************)
$(info >>> *** WARNING: Piksi INS was enabled, but image is not protected! ***)
$(info >>> *** WARNING: Piksi INS was enabled, but image is not protected! ***)
$(info >>> *** WARNING: Piksi INS was enabled, but image is not protected! ***)
$(info >>> *******************************************************************)
 
endif #  ($(BR2_BUILD_RELEASE_PROTECTED),y)

$(info *** Piksi INS is enabled, packaging with current image...)

PIKSI_INS_VERSION = v1.6
PIKSI_INS_SITE = git@github.com:carnegieroboticsllc/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = libuv libsbp libpiksi eigen

$(eval $(cmake-package))

endif # ($(BR2_HAS_PIKSI_INS),y)
