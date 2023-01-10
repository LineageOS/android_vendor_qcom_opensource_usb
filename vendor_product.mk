PRODUCT_SOONG_NAMESPACES += vendor/qcom/opensource/usb/etc

# USB init scripts
PRODUCT_PACKAGES += init.qcom.usb.rc init.qcom.usb.sh

# additional debugging on userdebug/eng builds
ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
  PRODUCT_PACKAGES += init.qti.usb.debug.sh
  PRODUCT_PACKAGES += init.qti.usb.debug.rc
endif

ifneq ($(TARGET_KERNEL_VERSION),$(filter $(TARGET_KERNEL_VERSION),4.9 4.14))
  PRODUCT_PACKAGES += android.hardware.usb@1.3-service-qti
endif

ifneq ($(filter taro kalama neo parrot anorak,$(TARGET_BOARD_PLATFORM)),)
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.use_gadget_hal=1
  PRODUCT_PACKAGES += android.hardware.usb.gadget@1.1-service-qti
  PRODUCT_PACKAGES += usb_compositions.conf
else
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.use_gadget_hal=0
endif

#
# Default property overrides for various function configurations
# These can be further overridden at runtime in init*.rc files as needed
#
ifneq ($(filter anorak,$(TARGET_BOARD_PLATFORM)),)
PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rndis.func.name=rndis
else
PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rndis.func.name=gsi
endif
PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rmnet.func.name=gsi
PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rmnet.inst.name=rmnet
PRODUCT_PROPERTY_OVERRIDES += vendor.usb.dpl.inst.name=dpl

# QDSS uses SW path on these targets
ifneq ($(filter lahaina taro parrot neo anorak ravelin,$(TARGET_BOARD_PLATFORM)),)
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.qdss.inst.name=qdss_sw
else
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.qdss.inst.name=qdss
endif

ifeq ($(TARGET_HAS_DIAG_ROUTER),true)
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.diag.func.name=ffs
else
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.diag.func.name=diag
endif

ifneq ($(TARGET_KERNEL_VERSION),$(filter $(TARGET_KERNEL_VERSION),4.9 4.14 4.19))
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.use_ffs_mtp=1
  PRODUCT_PROPERTY_OVERRIDES += sys.usb.mtp.batchcancel=1
else
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.use_ffs_mtp=0
endif
