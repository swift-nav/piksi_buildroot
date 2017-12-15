#############################################################
#
# piksi_ins
#
#############################################################
PIKSI_INS_VERSION = 7efb6c18316f673a8e43def2005f5c79b9c75d34
PIKSI_INS_SITE = ssh://git@endpoint.carnegierobotics.com/opt/git/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = czmq libsbp libpiksi eigen

$(eval $(cmake-package))