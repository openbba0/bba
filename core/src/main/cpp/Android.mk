LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := xdl
LOCAL_CXXFLAGS := -std=c++11 -fno-exceptions -fno-rtti
LOCAL_EXPORT_C_INCLUDES:=$(LOCAL_PATH)
LOCAL_SRC_FILES := xdl/xdl.c \
    xdl/xdl_iterate.c \
    xdl/xdl_linker.c \
    xdl/xdl_lzma.c \
    xdl/xdl_util.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)
include $(BUILD_STATIC_LIBRARY)

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)

include $(CLEAR_VARS)
LOCAL_MODULE := capstone
LOCAL_SRC_FILES := capstone/cs.c \
    capstone/utils.c \
    capstone/s_stream.c \
    capstone/mc_inst.c \
    capstone/mc_instr_desc.c \
    capstone/mc_register_info.c \
    capstone/mapping.c \
    capstone/arch/aarch64/aarch64_base_info.c \
    capstone/arch/aarch64/aarch64_disassembler.c \
    capstone/arch/aarch64/aarch64_inst_printer.c \
    capstone/arch/aarch64/aarch64_mapping.c \
    capstone/arch/aarch64/aarch64_module.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/capstone $(LOCAL_PATH)/capstone/include
LOCAL_CFLAGS := -O2 -w -fvisibility=hidden -D_GNU_SOURCE -DCAPSTONE_HAS_ARM64 -DCAPSTONE_USE_SYS_DYN_MEM
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := gum
LOCAL_SRC_FILES := \
    gum/arch-arm64/gumarm64reader.c \
    gum/arch-arm64/gumarm64relocator.c \
    gum/arch-arm64/gumarm64writer.c \
    gum/backend-arm64/gumcpucontext-arm64.c \
    gum/backend-arm64/guminterceptor-arm64-glue.S \
    gum/backend-arm64/guminterceptor-arm64.c \
    gum/backend-arm64/gumprocess-arm64.c \
    gum/backend-arm64/gumstalker-arm64-glue.S \
    gum/backend-arm64/gumstalker-arm64.c \
    gum/backend-arm64/memcpy-advsimd.S \
    gum/backend-arm64/compile_stack.c \
    gum/backend-arm64/compile_stack_arm64.S \
    gum/backend-elf/gummodule-elf.c \
    gum/backend-linux/gumandroid.c \
    gum/backend-linux/gummemory-linux.c \
    gum/backend-linux/gummodule-linux.c \
    gum/backend-linux/gumprocess-linux.c \
    gum/backend-posix/gumexceptor-posix.c \
    gum/backend-posix/gummemory-posix.c \
    gum/backend-posix/gumtls-posix.c \
    gum/rt/gruntime.c \
    gum/gum.c \
    gum/gumcloak.c \
    gum/gumcodeallocator.c \
    gum/gumcodesegment.c \
    gum/gumelfmodule.c \
    gum/gumenumtypes.c \
    gum/gumeventsink.c \
    gum/gumexceptor.c \
    gum/guminterceptor.c \
    gum/guminvocationcontext.c \
    gum/guminvocationlistener.c \
    gum/gumlibc.c \
    gum/gummemory.c \
    gum/gummemorymap.c \
    gum/gummetalarray.c \
    gum/gummetalhash.c \
    gum/gummodule.c \
    gum/gummodulefacade.c \
    gum/gummodulemap.c \
    gum/gumprintf.c \
    gum/gumprocess.c \
    gum/gumspinlock.c \
    gum/gumstalker.c \
    gum/spindle_str.c
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/capstone/include \
    $(LOCAL_PATH)/gum \
    $(LOCAL_PATH)/gum/rt \
    $(LOCAL_PATH)/gum/arch-arm64 \
    $(LOCAL_PATH)/gum/backend-arm64 \
    $(LOCAL_PATH)/gum/backend-elf \
    $(LOCAL_PATH)/gum/backend-linux/include
LOCAL_CFLAGS := -O2 -w -fvisibility=hidden -D_GNU_SOURCE \
    -DGUM_STATIC -DHAVE_ARM64=1 -DHAVE_LINUX=1 -DHAVE_ANDROID=1 -DHAVE_ELF=1 \
    -DHAVE_PACK_PRAGMA=1 -DHAVE_SYNC_LOCK=1 -DHAVE_CLEAR_CACHE=1 -DHAVE_CLTZ=1 \
    -DHAVE_POPCOUNT=1 -DHAVE_MADVISE=1 -DHAVE_POSIX_MADVISE=1 \
    -DHAVE_PTHREAD_ATTR_GETSTACK=1 -DHAVE_PTHREAD_SETNAME_NP=1 -DHAVE_LINK_H=1 \
    -DHAVE_STDINT_H=1 -DHAVE_SYS_AUXV_H=1 -DG_LOG_DOMAIN=\"BlackBox\"
LOCAL_STATIC_LIBRARIES := capstone
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := qjs
LOCAL_SRC_FILES := \
    qjs/cutils.c \
    qjs/dtoa.c \
    qjs/libregexp.c \
    qjs/libunicode.c \
    qjs/quickjs.c \
    js/qjs_binding.c \
    js/stalker_js.c \
    js/art_hook.c
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/qjs \
    $(LOCAL_PATH)/js \
    $(LOCAL_PATH)/gum \
    $(LOCAL_PATH)/gum/rt \
    $(LOCAL_PATH)/gum/arch-arm64 \
    $(LOCAL_PATH)/gum/backend-arm64 \
    $(LOCAL_PATH)/gum/backend-elf \
    $(LOCAL_PATH)/gum/backend-linux/include \
    $(LOCAL_PATH)/capstone/include
LOCAL_CFLAGS := -O2 -w -fvisibility=hidden -fwrapv -D_GNU_SOURCE \
    -DCONFIG_VERSION=\"2026-06-04\" -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
include $(BUILD_STATIC_LIBRARY)

endif

include $(CLEAR_VARS)
# Collect all source files in the current directory
SRC1 := $(wildcard $(LOCAL_PATH)/*.cpp) $(wildcard $(LOCAL_PATH)/*.c)
# Collect all source files in utils/
SRC2 := $(wildcard $(LOCAL_PATH)/utils/*.cpp) $(wildcard $(LOCAL_PATH)/utils/*.c)
# Collect all source files in hook/
SRC3 := $(wildcard $(LOCAL_PATH)/hook/*.cpp) $(wildcard $(LOCAL_PATH)/hook/*.c)
# Collect all source files in jnihook/
SRC4 := $(wildcard $(LOCAL_PATH)/jnihook/*.cpp) $(wildcard $(LOCAL_PATH)/jnihook/*.c)

LOCAL_MODULE := bba
LOCAL_SRC_FILES := box_core.cpp \
hidden_api.cpp \
io.cpp \
utils/elf_util.cpp \
hook/dex_file_hook.cpp \
hook/file_system_hook.cpp \
utils/virtual_spoof.cpp \
utils/hex_dump.cpp \
utils/anti_detection.cpp \
hook/vm_class_loader_hook.cpp \
hook/unix_file_system_hook.cpp \
hook/binder_hook.cpp \
hook/zlib_hook.cpp \
hook/base_hook.cpp \
hook/gum_engine.cpp \
hook/spindle_server.cpp \
jnihook/jni_hook.cpp

LOCAL_C_INCLUDES += $(LOCAL_PATH)
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/capstone/include \
    $(LOCAL_PATH)/gum \
    $(LOCAL_PATH)/gum/rt \
    $(LOCAL_PATH)/gum/arch-arm64 \
    $(LOCAL_PATH)/gum/backend-arm64 \
    $(LOCAL_PATH)/gum/backend-elf \
    $(LOCAL_PATH)/gum/backend-linux/include \
    $(LOCAL_PATH)/js
LOCAL_SRC_FILES += js/spindle_backtrace.cpp
endif
LOCAL_CFLAGS += -Wno-error=format-security -fvisibility=hidden -ffunction-sections -fdata-sections -w -std=c++17
LOCAL_CPPFLAGS += -Wno-error=format-security -fvisibility=hidden -ffunction-sections -fdata-sections -w -Werror -fms-extensions
LOCAL_LDFLAGS += -Wl,--gc-sections,--strip-all,-z,max-page-size=16384
LOCAL_ARM_MODE := arm

LOCAL_CPP_FEATURES := exceptions
LOCAL_STATIC_LIBRARIES := xdl
ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
LOCAL_STATIC_LIBRARIES += qjs gum capstone
endif
LOCAL_LDLIBS := -llog -landroid -lz -lm
include $(BUILD_SHARED_LIBRARY)
