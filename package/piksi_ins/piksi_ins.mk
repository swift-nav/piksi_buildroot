#############################################################
#
# piksi_ins
#
#############################################################
PIKSI_INS_VERSION = a83a49bce4adb172be5b96dd3751e2e2eb66cdd8
PIKSI_INS_SITE = ssh://git@portal/opt/git/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = czmq libsbp libpiksi eigen

$(eval $(cmake-package))