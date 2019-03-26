################################################################################
#
# rpmsg_piksi
#
################################################################################

RPMSG_PIKSI_VERSION = master
RPMSG_PIKSI_SITE = git@github.com:woodfell/rpmsg_piksi.git
RPMSG_PIKSI_SITE_METHOD = git

$(eval $(kernel-module))
$(eval $(generic-package))
