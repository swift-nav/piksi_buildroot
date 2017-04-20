################################################################################
#
# rpmsg_piksi
#
################################################################################

RPMSG_PIKSI_VERSION = 0.1
RPMSG_PIKSI_SITE = \
  "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/rpmsg_piksi/src"
RPMSG_PIKSI_SITE_METHOD = local

$(eval $(kernel-module))
$(eval $(generic-package))
