#############################################################
#
# piksi_ins
#
#############################################################
PIKSI_INS_VERSION = master
PIKSI_INS_SITE = git@github.com:carnegieroboticsllc/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = czmq libsbp libpiksi eigen

$(eval $(cmake-package))
