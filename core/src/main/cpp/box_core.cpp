



#include "box_core.h"
#include "log.h"
#include "io.h"
#include <jni.h>
#include <stdint.h>
#include <jnihook/jni_hook.h>
#include <hook/vm_class_loader_hook.h>
#include <hook/unix_file_system_hook.h>
#include <hook/file_system_hook.h>
#include <hook/binder_hook.h>
#include <hook/dex_file_hook.h>
#include <hook/runtime_hook.h>
#include <hook/zlib_hook.h>
#include <hook/gum_engine.h>
#include <hook/spindle_server.h>
#include "utils/hex_dump.h"
#include "hidden_api.h"

struct {
    JavaVM *vm;
    jclass NativeCoreClass;
    jmethodID getCallingUidId;
    jmethodID redirectPathString;
    jmethodID redirectPathFile;
    jmethodID loadEmptyDex;
    jmethodID loadEmptyDexL;
    int api_level;
} VMEnv;


JNIEnv *getEnv() {
    JNIEnv *env;
    VMEnv.vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    return env;
}

JNIEnv *ensureEnvCreated() {
    JNIEnv *env = getEnv();
    if (env == NULL) {
        VMEnv.vm->AttachCurrentThread(&env, NULL);
    }
    return env;
}

int BoxCore::getCallingUid(JNIEnv *env, int orig) {
    env = ensureEnvCreated();
    return env->CallStaticIntMethod(VMEnv.NativeCoreClass, VMEnv.getCallingUidId, orig);
}

jstring BoxCore::redirectPathString(JNIEnv *env, jstring path) {
    env = ensureEnvCreated();
    return (jstring) env->CallStaticObjectMethod(VMEnv.NativeCoreClass, VMEnv.redirectPathString, path);
}

jobject BoxCore::redirectPathFile(JNIEnv *env, jobject path) {
    env = ensureEnvCreated();
    return env->CallStaticObjectMethod(VMEnv.NativeCoreClass, VMEnv.redirectPathFile, path);
}

jlongArray BoxCore::loadEmptyDex(JNIEnv *env) {
    env = ensureEnvCreated();
    return (jlongArray) env->CallStaticObjectMethod(VMEnv.NativeCoreClass, VMEnv.loadEmptyDex);
}

int BoxCore::getApiLevel() {
    return VMEnv.api_level;
}

void BoxCore::onSpindleScript(const void *data, int len) {
    JNIEnv *env = ensureEnvCreated();
    if (env == nullptr || VMEnv.NativeCoreClass == nullptr || data == nullptr || len < 0) {
        return;
    }
    static jmethodID mid = nullptr;
    if (mid == nullptr) {
        mid = env->GetStaticMethodID(VMEnv.NativeCoreClass, "onSpindleScript", "([B)V");
    }
    if (mid == nullptr) {
        env->ExceptionClear();
        return;
    }
    jbyteArray bytes = env->NewByteArray(len);
    if (bytes == nullptr) {
        return;
    }
    env->SetByteArrayRegion(bytes, 0, len, reinterpret_cast<const jbyte *>(data));
    env->CallStaticVoidMethod(VMEnv.NativeCoreClass, mid, bytes);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(bytes);
}

bool BoxCore::launchForSpindle(const char *packageName) {
    JNIEnv *env = ensureEnvCreated();
    if (env == nullptr || VMEnv.NativeCoreClass == nullptr || packageName == nullptr) {
        return false;
    }
    jmethodID mid = env->GetStaticMethodID(VMEnv.NativeCoreClass, "launchForSpindle",
                                           "(Ljava/lang/String;)Z");
    if (mid == nullptr) {
        env->ExceptionClear();
        return false;
    }
    jstring name = env->NewStringUTF(packageName);
    if (name == nullptr) {
        return false;
    }
    jboolean ok = env->CallStaticBooleanMethod(VMEnv.NativeCoreClass, mid, name);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        ok = JNI_FALSE;
    }
    env->DeleteLocalRef(name);
    return ok == JNI_TRUE;
}

JavaVM *BoxCore::getJavaVM() {
    return VMEnv.vm;
}

void nativeHook(JNIEnv *env) {
    GumEngine::init();
    BaseHook::init(env);
    UnixFileSystemHook::init(env);
    FileSystemHook::init();
    VMClassLoaderHook::init(env);

    BinderHook::init(env);
    DexFileHook::init(env);
    ZlibHook::init(env);
}

void hideXposed(JNIEnv *env, jclass clazz) {
    ALOGD("set hideXposed");
    VMClassLoaderHook::hideXposed();
}

void init(JNIEnv *env, jobject clazz, jint api_level) {
    ALOGD("NativeCore init.");
    VMEnv.api_level = api_level;
    VMEnv.NativeCoreClass = (jclass) env->NewGlobalRef(env->FindClass(VMCORE_CLASS));
    VMEnv.getCallingUidId = env->GetStaticMethodID(VMEnv.NativeCoreClass, "getCallingUid", "(I)I");
    VMEnv.redirectPathString = env->GetStaticMethodID(VMEnv.NativeCoreClass, "redirectPath",
                                                      "(Ljava/lang/String;)Ljava/lang/String;");
    VMEnv.redirectPathFile = env->GetStaticMethodID(VMEnv.NativeCoreClass, "redirectPath",
                                                    "(Ljava/io/File;)Ljava/io/File;");
    VMEnv.loadEmptyDex = env->GetStaticMethodID(VMEnv.NativeCoreClass, "loadEmptyDex",
                                                "()[J");

    JniHook::InitJniHook(env, api_level);
    GumEngine::selfTest();
    SpindleServer::start();
}

void addIORule(JNIEnv *env, jclass clazz, jstring target_path,
               jstring relocate_path) {
    ALOGD("set addIORule");
    IO::addRule(env->GetStringUTFChars(target_path, JNI_FALSE),
                env->GetStringUTFChars(relocate_path, JNI_FALSE));
}

void enableIO(JNIEnv *env, jclass clazz) {
    ALOGD("set enableIO");
    IO::init(env);
    nativeHook(env);
}

bool disableHiddenApi(JNIEnv *env, jclass clazz) {
    ALOGD("set disableHiddenApi");
    if(!disable_hidden_api(env)){
        ALOGD("set disableHiddenApi Fail!!!");
        return false;
    }
    return true;
}

bool disableResourceLoading(JNIEnv *env, jclass clazz) {
    ALOGD("set disableResourceLoading");
    if(!disable_resource_loading()){
        ALOGD("set disableResourceLoading Fail!!!");
        return false;
    }
    return true;
}

jlong gumReplace(JNIEnv *env, jclass clazz, jlong target, jlong replacement) {
    (void) env;
    (void) clazz;
    void *original = nullptr;
    int rc = GumEngine::replace(reinterpret_cast<void *>(target),
                                reinterpret_cast<void *>(replacement), &original);
    if (rc != 0) {
        return 0;
    }
    return reinterpret_cast<jlong>(original);
}

void gumRevert(JNIEnv *env, jclass clazz, jlong target) {
    (void) env;
    (void) clazz;
    GumEngine::revert(reinterpret_cast<void *>(target));
}

jboolean gumStalkerFollow(JNIEnv *env, jclass clazz, jlong threadId, jint eventMask) {
    (void) env;
    (void) clazz;
    return GumEngine::stalkerFollow((uint64_t) threadId, (int) eventMask, nullptr, nullptr)
           ? JNI_TRUE : JNI_FALSE;
}

void gumStalkerUnfollow(JNIEnv *env, jclass clazz, jlong threadId) {
    (void) env;
    (void) clazz;
    GumEngine::stalkerUnfollow((uint64_t) threadId);
}

void gumStalkerExclude(JNIEnv *env, jclass clazz, jlong base, jlong size) {
    (void) env;
    (void) clazz;
    GumEngine::stalkerExclude((uintptr_t) base, (size_t) size);
}

jint gumStalkerEventCount(JNIEnv *env, jclass clazz) {
    (void) env;
    (void) clazz;
    return GumEngine::stalkerEventCount();
}

void spindleAgent(JNIEnv *env, jclass clazz, jstring name) {
    (void) clazz;
    if (name == nullptr) {
        return;
    }
    const char *chars = env->GetStringUTFChars(name, nullptr);
    if (chars != nullptr) {
        SpindleServer::join(chars);
        env->ReleaseStringUTFChars(name, chars);
    }
}

static JNINativeMethod gMethods[] = {
        {"disableHiddenApi", "()Z",                               (void *) disableHiddenApi},
        {"disableResourceLoading", "()Z",                         (void *) disableResourceLoading},
        {"hideXposed", "()V",                                     (void *) hideXposed},
        {"addIORule",  "(Ljava/lang/String;Ljava/lang/String;)V", (void *) addIORule},
        {"enableIO",   "()V",                                     (void *) enableIO},
        {"init",       "(I)V",                                    (void *) init},
        {"gumReplace", "(JJ)J",                                   (void *) gumReplace},
        {"gumRevert", "(J)V",                                     (void *) gumRevert},
        {"gumStalkerFollow", "(JI)Z",                             (void *) gumStalkerFollow},
        {"gumStalkerUnfollow", "(J)V",                            (void *) gumStalkerUnfollow},
        {"gumStalkerExclude", "(JJ)V",                            (void *) gumStalkerExclude},
        {"gumStalkerEventCount", "()I",                           (void *) gumStalkerEventCount},
        {"spindleAgent", "(Ljava/lang/String;)V",                 (void *) spindleAgent},
};

int registerNativeMethods(JNIEnv *env, const char *className,
                          JNINativeMethod *gMethods, int numMethods) {
    jclass clazz;
    clazz = env->FindClass(className);
    if (clazz == nullptr) {
        return JNI_FALSE;
    }
    if (env->RegisterNatives(clazz, gMethods, numMethods) < 0) {
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

int registerNatives(JNIEnv *env) {
    if (!registerNativeMethods(env, VMCORE_CLASS, gMethods,
                               sizeof(gMethods) / sizeof(gMethods[0])))
        return JNI_FALSE;
    return JNI_TRUE;
}

void registerMethod(JNIEnv *jenv) {
    registerNatives(jenv);
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    VMEnv.vm = vm;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_EVERSION;
    }
    registerMethod(env);
    return JNI_VERSION_1_6;
}