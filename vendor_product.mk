PRODUCT_SOONG_NAMESPACES += vendor/qcom/opensource/usb/etc

#
# Default property overrides for various function configurations
# These can be further overridden at runtime in init*.rc files as needed
#
ifneq ($(filter anorak niobe shikra,$(TARGET_BOARD_PLATFORM)),)
PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rndis.func.name=rndis
else
PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rndis.func.name=gsi
endif

PRODUCT_PROPERTY_OVERRIDES += vendor.usb.dpl.inst.name=dpl

# A2 BAM DEMUX  path on these targets
ifneq ($(filter shikra,$(TARGET_BOARD_PLATFORM)),)
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rmnet.func.name=rmnet_bam
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rmnet.inst.name=rmnet_bam_dmux
else
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rmnet.func.name=gsi
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.rmnet.inst.name=rmnet
endif

ifneq ($(filter blair monaco bengal malabar shikra trinket pitti,$(TARGET_BOARD_PLATFORM)),)
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.controller=4e00000.dwc3
  ifneq ($(filter bengal trinket,$(TARGET_BOARD_PLATFORM)),)
    PRODUCT_SYSTEM_PROPERTIES += ro.boot.usb.dwc3_msm=4e00000.ssusb
  else
    PRODUCT_SYSTEM_PROPERTIES += ro.boot.usb.dwc3_msm=4e00000.hsusb
  endif
else
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.controller=a600000.dwc3
endif

# NCM uses HW path on these targets
ifneq ($(filter pikachu seraph,$(TARGET_BOARD_PLATFORM)),)
PRODUCT_PROPERTY_OVERRIDES += vendor.usb.ncm.func.name=gsi
endif

# QDSS uses SW path on these targets
ifneq ($(filter kona pikachu seraph lahaina taro anorak neo blair kalama pineapple sun monaco parrot canoe vienna chora bengal malabar shikra trinket tuna kera crow pitti niobe volcano anorak61 neo61,$(TARGET_BOARD_PLATFORM)),)
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

ifneq ($(TARGET_KERNEL_VERSION),$(filter $(TARGET_KERNEL_VERSION),4.9 4.14))
  PRODUCT_PACKAGES += android.hardware.usb-service.qti
endif

PRODUCT_VENDOR_PROPERTIES += ro.usb.uvc.enabled=true

USB_USES_QMAA = $(TARGET_USES_QMAA)
ifeq ($(TARGET_USES_QMAA_OVERRIDE_USB),true)
       USB_USES_QMAA = false
endif

# USB init scripts
ifeq ($(USB_USES_QMAA),true)
  PRODUCT_PACKAGES += init.qti.usb.qmaa.rc
else
  PRODUCT_PACKAGES += init.qcom.usb.rc init.qcom.usb.sh

  #
  # USB Gadget HAL is enabled on newer targets and takes the place
  # of the init-based configfs rules for setting USB compositions
  #
  ifneq ($(filter kona pikachu seraph niobe taro anorak neo kalama pineapple sun parrot canoe monaco vienna lahaina chora bengal malabar shikra hamoa blair trinket crow pitti volcano anorak61 neo61,$(TARGET_BOARD_PLATFORM)),)
    PRODUCT_PROPERTY_OVERRIDES += vendor.usb.use_gadget_hal=1
    PRODUCT_PACKAGES += android.hardware.usb.gadget-service.qti
    PRODUCT_PACKAGES += usb_compositions.conf
  else
    PRODUCT_PROPERTY_OVERRIDES += vendor.usb.use_gadget_hal=0
  endif

endif

# additional debugging on userdebug/eng builds
ifneq (,$(filter userdebug eng, $(TARGET_BUILD_VARIANT)))
  PRODUCT_PACKAGES += init.qti.usb.debug.sh
  PRODUCT_PACKAGES += init.qti.usb.debug.rc
endif
