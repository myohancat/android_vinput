LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    main.cpp \
    vkey.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/ 

COMMON_STATIC_LIBRARIES := \

#LOCAL_CFLAGS += -Wall -Wextra -Werror
#LOCAL_CFLAGS += -Wno-unused-parameter
#LOCAL_CFLAGS += -frtti -fexceptions -std=c++11
LOCAL_CFLAGS += -fPIE -pie

LOCAL_MODULE := vinput

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)
