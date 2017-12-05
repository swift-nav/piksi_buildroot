#############################################################
#
# piksi_ins
#
#############################################################
PIKSI_INS_VERSION = a04a36fd25ade13f8adb980c80c5b28074e534ee
PIKSI_INS_SITE = ssh://git@endpoint.carnegierobotics.com/opt/git/piksi_ins.git
PIKSI_INS_SITE_METHOD = git
PIKSI_INS_INSTALL_STAGING = YES
PIKSI_INS_INSTALL_TARGET = YES
PIKSI_INS_DEPENDENCIES = czmq libsbp libpiksi eigen

$(eval $(cmake-package))