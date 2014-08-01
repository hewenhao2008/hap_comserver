#
# This software is licensed under the Public Domain.
#
include $(TOPDIR)/rules.mk

PKG_NAME:=hap_comserver
PKG_VERSION:=0.1
PKG_RELEASE:=1

include $(INCLUDE_DIR)/package.mk

define Package/hap_comserver
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=hap_comserver Deamon
	DEPENDS:=+libpthread +librt
endef

define Package/hap_comserver/description
	This is some dummy application.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/hap_comserver/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/hap_comserver $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc/init.d/
	$(INSTALL_BIN) files/hap_comserver.initd $(1)/etc/init.d/hap_comserver
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) files/hap_comserver.cfg $(1)/etc/config/hap_comserver
endef

$(eval $(call BuildPackage,hap_comserver))
