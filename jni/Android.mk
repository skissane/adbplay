LOCAL_PATH:= $(call my-dir)


include $(CLEAR_VARS)
LOCAL_CFLAGS += -ferror-limit=1 -Wall -Wextra -Werror
LOCAL_C_INCLUDES:= $(LOCAL_PATH)/include/
LOCAL_SRC_FILES:= ../adbplay.c
LOCAL_MODULE := adbplay
LOCAL_SHARED_LIBRARIES:=
LOCAL_MODULE_TAGS := optional
LOCAL_LDLIBS := -lOpenSLES -llog

APP_PLATFORM := android-16
include $(BUILD_EXECUTABLE)
