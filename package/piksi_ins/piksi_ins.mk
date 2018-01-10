#############################################################
#
# piksi_ins
#
#############################################################
PIKSI_INS_VERSION = bcf0ae1dc40270f54bfa7c3858ddc01e9507477e
PIKSI_INS_SITE = ssh://git@endpoint.carnegierobotics.com/opt/git/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = czmq libsbp libpiksi eigen

$(eval $(cmake-package))