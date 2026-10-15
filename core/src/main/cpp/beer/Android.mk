LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := beer

LOCAL_SRC_FILES := \
    sandhook.cpp \
    trampoline/trampoline.cpp \
    trampoline/trampoline_manager.cpp \
    utils/dlfcn_nougat.cpp \
    utils/hide_api.cpp \
    utils/utils.cpp \
    utils/offset.cpp \
    casts/cast_art_method.cpp \
    casts/cast_compiler_options.cpp \
    art/art_method.cpp \
    art/art_compiler_options.cpp \
    trampoline/arch/arm64.S \
    inst/insts_arm64.cpp \
    nativehook/native_hook.cpp

# ElfImg comes from core (utils/elf_util.cpp) to avoid duplicate SandHook::ElfImg.
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/includes \
    $(LOCAL_PATH)

LOCAL_CFLAGS := -O2 -w -fvisibility=hidden -D_GNU_SOURCE -DSANDHOOK_STATIC_LINK
LOCAL_CPPFLAGS := -std=c++17 -fvisibility=hidden -DSANDHOOK_STATIC_LINK
LOCAL_ARM_MODE := arm
# Core ElfImg (string_view) and ART hooks need libc++.
LOCAL_CPP_FEATURES := exceptions rtti

include $(BUILD_STATIC_LIBRARY)
