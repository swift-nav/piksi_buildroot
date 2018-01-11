################################################################################
#
# MTD_RW
#
################################################################################

MTDRW_VERSION = 1.0
MTDRW_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/mtdrw"
MTDRW_SITE_METHOD = local
$(eval $(kernel-module))
$(eval $(generic-package))