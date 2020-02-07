PRODUCT_PACKAGES += android.hardware.usb@1.1-service-qti

ifeq ($(TARGET_USES_USB_GADGET_HAL), true)
  PRODUCT_PACKAGES += android.hardware.usb.gadget@1.0-service-qti
endif
