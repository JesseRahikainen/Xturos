LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL2
NUKLEAR_PATH := ../nuklear
STB_PATH := ../stb
SPINE_PATH := ../spine_c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include \
	$(LOCAL_PATH)/$(NUKLEAR_PATH) \
	$(LOCAL_PATH)/$(STB_PATH) \
	$(LOCAL_PATH)/$(SPINE_PATH)/include

# Make does not offer a recursive wildcard function, so here's one:
#traverse all the directory and subdirectory
rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

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
					$(call rwildcard,$(LOCAL_PATH)/src/Game,*.c) \
					)


SPINE_SRC_FILES := $(subst $(LOCAL_PATH)/,, $(call rwildcard,$(LOCAL_PATH)/$(SPINE_PATH)/src/spine/,*.c))

#$(info game_src = $(GAME_SRC_FILES))

# Add your application source files here...
LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.c \
	$(GAME_SRC_FILES) $(SPINE_SRC_FILES)


LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv3 -llog

include $(BUILD_SHARED_LIBRARY)
