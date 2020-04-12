LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL2
NUKLEAR_PATH = F:/Data/Libraries/nuklear-master
STB_PATH = F:/Data/Libraries/stb-master
SPINE_PATH = F:/Data/Libraries/spine-runtimes-master/spine-c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include \
	$(NUKLEAR_PATH) \
	$(STB_PATH) \
	$(SPINE_PATH)/include

# need to exclude those from the Others folder
GAME_SRC_FILES := $(subst $(LOCAL_PATH)/,, $(wildcard $(LOCAL_PATH)/src/*.c) \
					$(wildcard $(LOCAL_PATH)/src/Graphics/*.c) \
					$(wildcard $(LOCAL_PATH)/src/IMGUI/*.c) \
					$(wildcard $(LOCAL_PATH)/src/Input/*.c) \
					$(wildcard $(LOCAL_PATH)/src/Math/*.c) \
					$(wildcard $(LOCAL_PATH)/src/System/*.c) \
					$(wildcard $(LOCAL_PATH)/src/System/ECPS/*.c) \
					$(wildcard $(LOCAL_PATH)/src/UI/*.c) \
					$(wildcard $(LOCAL_PATH)/src/Utils/*.c) \
					$(wildcard $(LOCAL_PATH)/src/Components/*.c) \
					$(wildcard $(LOCAL_PATH)/src/Processes/*.c) \
					$(wildcard $(LOCAL_PATH)/src/Game/*.c) \
					)

SPINE_SRC_FILES := $(wildcard $(LOCAL_PATH)/../spine_c/src/spine/*.c)

# Add your application source files here...
LOCAL_SRC_FILES := $(SPINE_SRC_FILES) $(GAME_SRC_FILES)

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lGLESv3 -llog

include $(BUILD_SHARED_LIBRARY)
