################################################################################
#
# bluez5_utils
#
################################################################################

SWIFT_BLUEZ_VERSION = 5.43
SWIFT_BLUEZ_SITE = "${BR2_EXTERNAL_piksi_buildroot_PATH}/package/swift_bluez/swift_bluez"
SWIFT_BLUEZ_SITE_METHOD = local
SWIFT_BLUEZ_INSTALL_STAGING = YES
SWIFT_BLUEZ_DEPENDENCIES = dbus libglib2 readline zlib
SWIFT_BLUEZ_AUTORECONF=YES
SWIFT_BLUEZ_CONF_OPTS = \
	--enable-tools \
	--enable-library \
	--disable-cups \
	--with-z

SWIFT_BLUEZ_CONF_ENV += \
	LDFLAGS='$(TARGET_LDFLAGS) -lz'

ifeq ($(BR2_PACKAGE_SWIFT_BLUEZ_OBEX),y)
SWIFT_BLUEZ_CONF_OPTS += --enable-obex
SWIFT_BLUEZ_DEPENDENCIES += libical
else
SWIFT_BLUEZ_CONF_OPTS += --disable-obex
endif

ifeq ($(BR2_PACKAGE_SWIFT_BLUEZ_CLIENT),y)
SWIFT_BLUEZ_CONF_OPTS += --enable-client
SWIFT_BLUEZ_DEPENDENCIES += readline
else
SWIFT_BLUEZ_CONF_OPTS += --disable-client
endif

# experimental plugins
ifeq ($(BR2_PACKAGE_SWIFT_BLUEZ_EXPERIMENTAL),y)
SWIFT_BLUEZ_CONF_OPTS += --enable-experimental
else
SWIFT_BLUEZ_CONF_OPTS += --disable-experimental
endif

# enable health plugin
ifeq ($(BR2_PACKAGE_BLUEZ5_PLUGINS_HEALTH),y)
SWIFT_BLUEZ_CONF_OPTS += --enable-health
else
SWIFT_BLUEZ_CONF_OPTS += --disable-health
endif

# enable midi profile
ifeq ($(BR2_PACKAGE_BLUEZ5_PLUGINS_MIDI),y)
SWIFT_BLUEZ_CONF_OPTS += --enable-midi
SWIFT_BLUEZ_DEPENDENCIES += alsa-lib
else
SWIFT_BLUEZ_CONF_OPTS += --disable-midi
endif

# enable nfc plugin
ifeq ($(BR2_PACKAGE_BLUEZ5_PLUGINS_NFC),y/configure)
SWIFT_BLUEZ_CONF_OPTS += --enable-nfc
else
SWIFT_BLUEZ_CONF_OPTS += --disable-nfc
endif

# enable sap plugin
ifeq ($(BR2_PACKAGE_BLUEZ5_PLUGINS_SAP),y)
SWIFT_BLUEZ_CONF_OPTS += --enable-sap
else
SWIFT_BLUEZ_CONF_OPTS += --disable-sap
endif

# enable sixaxis plugin
# ifeq ($(BR2_PACKAGE_BLUEZ5_PLUGINS_SIXAXIS),y)
# SWIFT_BLUEZ_CONF_OPTS += --enable-sixaxis
# else
# SWIFT_BLUEZ_CONF_OPTS += --disable-sixaxis
# endif

# install gatttool (For some reason upstream choose not to do it by default)
ifeq ($(BR2_PACKAGE_SWIFT_BLUEZ_DEPRECATED),y)
define SWIFT_BLUEZ_INSTALL_GATTTOOL
	$(INSTALL) -D -m 0755 $(@D)/attrib/gatttool $(TARGET_DIR)/usr/bin/gatttool
endef
SWIFT_BLUEZ_POST_INSTALL_TARGET_HOOKS += SWIFT_BLUEZ_INSTALL_GATTTOOL
# hciattach_bcm43xx defines default firmware path in `/etc/firmware`, but
# Broadcom firmware blobs are usually located in `/lib/firmware`.
SWIFT_BLUEZ_CONF_ENV += \
	CPPFLAGS='$(TARGET_CPPFLAGS) -DFIRMWARE_DIR=\"/lib/firmware\"'
SWIFT_BLUEZ_CONF_OPTS += --enable-deprecated
# else
# SWIFT_BLUEZ_CONF_OPTS += --disable-deprecated
endif

# enable test
ifeq ($(BR2_PACKAGE_SWIFT_BLUEZ_TEST),y)
SWIFT_BLUEZ_CONF_OPTS += --enable-test
else
SWIFT_BLUEZ_CONF_OPTS += --disable-test
endif

# use udev if available
ifeq ($(BR2_PACKAGE_HAS_UDEV),y)
SWIFT_BLUEZ_CONF_OPTS += --enable-udev
SWIFT_BLUEZ_DEPENDENCIES += udev
else
SWIFT_BLUEZ_CONF_OPTS += --disable-udev
endif

# integrate with systemd if available
ifeq ($(BR2_PACKAGE_SYSTEMD),y)
SWIFT_BLUEZ_CONF_OPTS += --enable-systemd
SWIFT_BLUEZ_DEPENDENCIES += systemd
else
SWIFT_BLUEZ_CONF_OPTS += --disable-systemd
endif

define SWIFT_BLUEZ_INSTALL_INIT_SYSTEMD
	mkdir -p $(TARGET_DIR)/etc/systemd/system/bluetooth.target.wants
	ln -fs ../../../../usr/lib/systemd/system/bluetooth.service \
		$(TARGET_DIR)/etc/systemd/system/bluetooth.target.wants/bluetooth.service
	ln -fs ../../../../usr/lib/systemd/system/bluetooth.service \
		$(TARGET_DIR)/etc/systemd/system/dbus-org.bluez.service
endef

$(eval $(autotools-package))
