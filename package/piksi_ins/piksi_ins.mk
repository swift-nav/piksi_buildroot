#############################################################
#
# piksi_ins
#
#############################################################
PIKSI_INS_VERSION = 4680358073d5dfc02909517c443ccd902985ff3b
PIKSI_INS_SITE = ssh://git@endpoint.carnegierobotics.com/opt/git/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = czmq libsbp libpiksi eigen

$(eval $(cmake-package))
