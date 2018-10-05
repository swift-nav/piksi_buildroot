################################################################################
#
# gnss_convertors
#
################################################################################

<<<<<<< HEAD
GNSS_CONVERTORS_VERSION = v0.3.61.6
=======
GNSS_CONVERTORS_VERSION = v0.3.65
>>>>>>> 10fce301... Update libsbp to 2.4.1 (#903)
GNSS_CONVERTORS_SITE = https://github.com/swift-nav/gnss-converters
GNSS_CONVERTORS_SITE_METHOD = git
GNSS_CONVERTORS_INSTALL_STAGING = YES
GNSS_CONVERTORS_SUBDIR = c
GNSS_CONVERTORS_DEPENDENCIES = libswiftnav librtcm libsbp

$(eval $(cmake-package))
