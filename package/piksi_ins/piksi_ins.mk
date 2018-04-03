#############################################################
#
# piksi_ins
#
#############################################################
PIKSI_INS_VERSION = d660c9b43422dbd4e5489a5c2a371d537242eea4
PIKSI_INS_SITE = ssh://git@endpoint.carnegierobotics.com/opt/git/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = czmq libsbp libpiksi eigen

$(eval $(cmake-package))