ifeq ($(TARGET_HAS_DIAG_ROUTER),true)
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.diag.func.name=ffs
else
  PRODUCT_PROPERTY_OVERRIDES += vendor.usb.diag.func.name=diag
endif

ifneq ($(TARGET_KERNEL_VERSION),$(filter $(TARGET_KERNEL_VERSION),4.9 4.14))
  PRODUCT_PACKAGES += android.hardware.usb@1.1-service-qti
endif

ifeq ($(TARGET_USES_USB_GADGET_HAL), true)
  PRODUCT_PACKAGES += android.hardware.usb.gadget@1.0-service-qti
endif
