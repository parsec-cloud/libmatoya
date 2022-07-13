LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LIBS_PATH := ../bin/android/$(TARGET_ARCH_ABI)

include $(CLEAR_VARS)
LOCAL_MODULE := libmatoya
LOCAL_SRC_FILES := $(LIBS_PATH)/libmatoya.a
include $(PREBUILT_STATIC_LIBRARY)

LOCAL_CFLAGS = \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-fPIC

LOCAL_SRC_FILES        := $(name).c
LOCAL_C_INCLUDES       := ../src gen
LOCAL_STATIC_LIBRARIES := libmatoya

LOCAL_LDLIBS := \
	-lGLESv3 \
	-lEGL \
	-lmediandk \
	-llog \
	-landroid \
	-laaudio

LOCAL_MODULE_FILENAME := libmain
LOCAL_MODULE          := libmain

include $(BUILD_SHARED_LIBRARY)
