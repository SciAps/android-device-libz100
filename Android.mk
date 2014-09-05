LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifneq ($(filter libz100,$(TARGET_DEVICE)),)
include $(call all-makefiles-under,$(LOCAL_PATH))
endif

