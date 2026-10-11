#include "ZlibHook.h"
#include "Log.h"
#include "xdl.h"
#include "Dobby/dobby.h"

#include <zlib.h>
#include <cstring>
#include <cstdio>
#include <ctime>

static const char *kTargetPackage = "com.xingin.xhs";
static const char *kMarker = "x98";

static int (*orig_deflate)(z_streamp strm, int flush) = nullptr;
static char g_dump_dir[1024] = {0};
static bool g_is_xhs = false;

static char *find_marker(const char *data, const char *marker, size_t len) {
    size_t marker_len = strlen(marker);
    if (marker_len == 0) {
        return const_cast<char *>(data);
    }
    while (len >= marker_len) {
        if (memcmp(data, marker, marker_len) == 0) {
            return const_cast<char *>(data);
        }
        data++;
        len--;
    }
    return nullptr;
}

static bool copy_static_string(JNIEnv *env, jclass clazz, const char *method, char *out, size_t out_len) {
    if (out_len == 0) {
        return false;
    }
    out[0] = '\0';
    jmethodID method_id = env->GetStaticMethodID(clazz, method, "()Ljava/lang/String;");
    if (method_id == nullptr) {
        env->ExceptionClear();
        return false;
    }
    jstring value = static_cast<jstring>(env->CallStaticObjectMethod(clazz, method_id));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    if (value == nullptr) {
        return false;
    }
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) {
        env->DeleteLocalRef(value);
        return false;
    }
    strncpy(out, chars, out_len - 1);
    out[out_len - 1] = '\0';
    env->ReleaseStringUTFChars(value, chars);
    env->DeleteLocalRef(value);
    return out[0] != '\0';
}

static bool call_static_boolean(JNIEnv *env, jclass clazz, const char *method) {
    jmethodID method_id = env->GetStaticMethodID(clazz, method, "()Z");
    if (method_id == nullptr) {
        env->ExceptionClear();
        return false;
    }
    jboolean value = env->CallStaticBooleanMethod(clazz, method_id);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return false;
    }
    return value == JNI_TRUE;
}

static void save_data_to_file(const Bytef *data, uInt len) {
    if (g_dump_dir[0] == '\0') {
        ALOGE("ZlibHook: dump dir is empty, skip dump");
        return;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    char filename[1280];
    snprintf(filename, sizeof(filename), "%s/xhs_ddd%ld-%ld", g_dump_dir, static_cast<long>(ts.tv_sec), static_cast<long>(ts.tv_nsec));
    FILE *fp = fopen(filename, "wb");
    if (fp == nullptr) {
        ALOGE("ZlibHook: Failed to open %s", filename);
        return;
    }
    fwrite(data, 1, len, fp);
    fclose(fp);
    ALOGD("ZlibHook: Saved %u bytes to %s", len, filename);
}

static int new_deflate(z_streamp strm, int flush) {
    if (g_is_xhs && strm != nullptr && strm->next_in != nullptr && strm->avail_in > 0) {
        if (find_marker(reinterpret_cast<const char *>(strm->next_in), kMarker, strm->avail_in) != nullptr) {
            save_data_to_file(strm->next_in, strm->avail_in);
        }
    }
    if (orig_deflate == nullptr) {
        return Z_STREAM_ERROR;
    }
    return orig_deflate(strm, flush);
}

void ZlibHook::init(JNIEnv *env) {
    jclass clazz = env->FindClass("com/android/bb/core/NativeCore");
    if (clazz == nullptr) {
        env->ExceptionClear();
        ALOGE("ZlibHook: NativeCore class not found");
        return;
    }
    char package_name[256] = {0};
    bool dump_enabled = call_static_boolean(env, clazz, "isXhsFingerprintDumpEnabled");
    if (copy_static_string(env, clazz, "getAppPackageName", package_name, sizeof(package_name))) {
        g_is_xhs = dump_enabled && strcmp(package_name, kTargetPackage) == 0;
    }
    copy_static_string(env, clazz, "getFingerprintDumpDir", g_dump_dir, sizeof(g_dump_dir));
    env->DeleteLocalRef(clazz);

    if (!g_is_xhs) {
        ALOGD("ZlibHook: skip libz hook, package=%s dump=%d",
              package_name[0] != '\0' ? package_name : "(empty)", dump_enabled);
        return;
    }

    void *handle = xdl_open("libz.so", XDL_DEFAULT);
    if (handle == nullptr) {
        ALOGE("ZlibHook: Failed to open libz.so");
        return;
    }
    void *deflate_addr = xdl_sym(handle, "deflate", nullptr);
    if (deflate_addr == nullptr) {
        ALOGE("ZlibHook: Failed to find deflate");
        xdl_close(handle);
        return;
    }
    if (DobbyHook(deflate_addr, reinterpret_cast<void *>(new_deflate), reinterpret_cast<void **>(&orig_deflate)) != 0) {
        ALOGE("ZlibHook: Failed to hook deflate");
        xdl_close(handle);
        return;
    }
    xdl_close(handle);
    ALOGD("ZlibHook: deflate hooked, dump dir=%s", g_dump_dir);
}
