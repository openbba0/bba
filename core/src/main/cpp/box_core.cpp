#include "box_core.h"
#include "log.h"
#include "io.h"
#include "sandhook_jni.h"
#include <jni.h>
#include <stdint.h>
#include <jnihook/jni_hook.h>
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

#include <pthread.h>
#include <cstdlib>
#include <cstring>

JNIEnv *ensureEnvCreated();

struct {
    JavaVM *vm;
    jclass NativeCoreClass;
    jmethodID getCallingUidId;
    jmethodID redirectPathString;
    jmethodID redirectPathFile;
    int api_level;
} VMEnv;

#if defined(__aarch64__)
extern "C" {
int spindle_run_js(const char *source, int source_len);
int spindle_rpc_call(const char *json, int json_len, char **out);
}
#endif

static pthread_mutex_t g_unlock_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_unlock_cv = PTHREAD_COND_INITIALIZER;
static int g_unlock_flag = 0;

extern "C" void spindle_set_unlock(void) {
    pthread_mutex_lock(&g_unlock_mu);
    g_unlock_flag = 1;
    pthread_cond_broadcast(&g_unlock_cv);
    pthread_mutex_unlock(&g_unlock_mu);
}

extern "C" void spindle_reset_unlock(void) {
    pthread_mutex_lock(&g_unlock_mu);
    g_unlock_flag = 0;
    pthread_mutex_unlock(&g_unlock_mu);
}

extern "C" void spindle_wait_unlock(void) {
    pthread_mutex_lock(&g_unlock_mu);
    while (!g_unlock_flag) {
        pthread_cond_wait(&g_unlock_cv, &g_unlock_mu);
    }
    pthread_mutex_unlock(&g_unlock_mu);
}

extern "C" void spindle_log(const char *text) {
    if (text == nullptr) {
        return;
    }
    __android_log_print(ANDROID_LOG_INFO, "SPDL", "%s", text);
    JNIEnv *env = ensureEnvCreated();
    if (env == nullptr || VMEnv.NativeCoreClass == nullptr) {
        return;
    }
    static jmethodID mid = nullptr;
    if (mid == nullptr) {
        mid = env->GetStaticMethodID(VMEnv.NativeCoreClass, "onSpindleLog",
                                     "(Ljava/lang/String;)V");
        if (mid == nullptr) {
            env->ExceptionClear();
            return;
        }
    }
    jstring line = env->NewStringUTF(text);
    if (line == nullptr) {
        return;
    }
    env->CallStaticVoidMethod(VMEnv.NativeCoreClass, mid, line);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    env->DeleteLocalRef(line);
}

static jclass gSpindleClass = nullptr;
static jmethodID gSpindleDeliver = nullptr;
static jmethodID gSpindleLaunch = nullptr;
static jmethodID gSpindleLaunchMain = nullptr;


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

int BoxCore::getApiLevel() {
    return VMEnv.api_level;
}

void BoxCore::bindSpindle(JNIEnv *env) {
    if (gSpindleClass != nullptr || env == nullptr) {
        return;
    }
    jclass local = env->FindClass("com/android/bb/core/system/spindle/BSpindleService");
    if (local == nullptr) {
        env->ExceptionClear();
        return;
    }
    gSpindleClass = reinterpret_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    gSpindleDeliver = env->GetStaticMethodID(gSpindleClass, "deliver", "(I[B)[B");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    gSpindleLaunch = env->GetStaticMethodID(gSpindleClass, "launch", "(Ljava/lang/String;)Z");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    gSpindleLaunchMain = env->GetStaticMethodID(gSpindleClass, "launchOnMain", "(Ljava/lang/String;)Z");
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

bool BoxCore::deliverSpindle(int id, const void *data, int len, std::string *reply) {
    JNIEnv *env = ensureEnvCreated();
    if (env == nullptr || gSpindleClass == nullptr || gSpindleDeliver == nullptr || data == nullptr || len < 0) {
        return false;
    }
    jbyteArray bytes = env->NewByteArray(len);
    if (bytes == nullptr) {
        return false;
    }
    if (len > 0) {
        env->SetByteArrayRegion(bytes, 0, len, reinterpret_cast<const jbyte *>(data));
    }
    jobject result = env->CallStaticObjectMethod(gSpindleClass, gSpindleDeliver, id, bytes);
    env->DeleteLocalRef(bytes);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        if (result != nullptr) {
            env->DeleteLocalRef(result);
        }
        return false;
    }
    if (result == nullptr) {
        return false;
    }
    auto *array = reinterpret_cast<jbyteArray>(result);
    jsize n = env->GetArrayLength(array);
    if (reply != nullptr) {
        reply->resize(static_cast<size_t>(n));
        if (n > 0) {
            env->GetByteArrayRegion(array, 0, n, reinterpret_cast<jbyte *>(&(*reply)[0]));
        }
    }
    env->DeleteLocalRef(result);
    return true;
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

static bool callSpindleLaunch(jmethodID mid, const char *packageName) {
    JNIEnv *env = ensureEnvCreated();
    if (env == nullptr || gSpindleClass == nullptr || mid == nullptr || packageName == nullptr) {
        return false;
    }
    jstring name = env->NewStringUTF(packageName);
    if (name == nullptr) {
        return false;
    }
    jboolean ok = env->CallStaticBooleanMethod(gSpindleClass, mid, name);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        ok = JNI_FALSE;
    }
    env->DeleteLocalRef(name);
    return ok == JNI_TRUE;
}

bool BoxCore::launchForSpindle(const char *packageName) {
    return callSpindleLaunch(gSpindleLaunch, packageName);
}

bool BoxCore::launchForSpindleOnMain(const char *packageName) {
    return callSpindleLaunch(gSpindleLaunchMain, packageName);
}

JavaVM *BoxCore::getJavaVM() {
    return VMEnv.vm;
}

extern "C" JavaVM *spindle_java_vm(void) {
    return VMEnv.vm;
}

void nativeHook(JNIEnv *env) {
    GumEngine::init();
    BaseHook::init(env);
    UnixFileSystemHook::init(env);
    FileSystemHook::init();

    BinderHook::init(env);
    DexFileHook::init(env);
    ZlibHook::init(env);
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

    JniHook::InitJniHook(env, api_level);
}

void setupIO(JNIEnv *env, jclass clazz, jobject rules) {
    ALOGD("set setupIO");
    (void) clazz;
    if (rules != nullptr) {
        jclass mapClass = env->GetObjectClass(rules);
        jmethodID entrySet = env->GetMethodID(mapClass, "entrySet", "()Ljava/util/Set;");
        jobject set = env->CallObjectMethod(rules, entrySet);
        if (set != nullptr) {
            jclass setClass = env->GetObjectClass(set);
            jmethodID iterator = env->GetMethodID(setClass, "iterator", "()Ljava/util/Iterator;");
            jobject it = env->CallObjectMethod(set, iterator);
            if (it != nullptr) {
                jclass itClass = env->GetObjectClass(it);
                jmethodID hasNext = env->GetMethodID(itClass, "hasNext", "()Z");
                jmethodID next = env->GetMethodID(itClass, "next", "()Ljava/lang/Object;");
                jclass entryClass = env->FindClass("java/util/Map$Entry");
                jmethodID getKey = env->GetMethodID(entryClass, "getKey", "()Ljava/lang/Object;");
                jmethodID getValue = env->GetMethodID(entryClass, "getValue", "()Ljava/lang/Object;");
                while (env->CallBooleanMethod(it, hasNext)) {
                    jobject entry = env->CallObjectMethod(it, next);
                    if (entry == nullptr) {
                        continue;
                    }
                    auto key = (jstring) env->CallObjectMethod(entry, getKey);
                    auto value = (jstring) env->CallObjectMethod(entry, getValue);
                    if (key != nullptr && value != nullptr) {
                        const char *from = env->GetStringUTFChars(key, nullptr);
                        const char *to = env->GetStringUTFChars(value, nullptr);
                        if (from != nullptr && to != nullptr) {
                            IO::addRule(from, to);
                        }
                        if (from != nullptr) env->ReleaseStringUTFChars(key, from);
                        if (to != nullptr) env->ReleaseStringUTFChars(value, to);
                    }
                    if (key != nullptr) env->DeleteLocalRef(key);
                    if (value != nullptr) env->DeleteLocalRef(value);
                    env->DeleteLocalRef(entry);
                }
                env->DeleteLocalRef(entryClass);
                env->DeleteLocalRef(itClass);
                env->DeleteLocalRef(it);
            }
            env->DeleteLocalRef(setClass);
            env->DeleteLocalRef(set);
        }
        env->DeleteLocalRef(mapClass);
    }
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

void spindleListen(JNIEnv *env, jclass clazz) {
    (void) clazz;
    BoxCore::bindSpindle(env);
    SpindleServer::start();
}

jint spindleAdd(JNIEnv *env, jclass clazz, jstring name, jint pid, jint uid) {
    (void) clazz;
    if (name == nullptr) {
        return 0;
    }
    const char *chars = env->GetStringUTFChars(name, nullptr);
    if (chars == nullptr) {
        return 0;
    }
    int id = SpindleServer::addAgent(chars, pid, uid);
    env->ReleaseStringUTFChars(name, chars);
    return id;
}

void spindleDrop(JNIEnv *env, jclass clazz, jint id) {
    (void) env;
    (void) clazz;
    SpindleServer::dropAgent(id);
}

jint spindleRunJs(JNIEnv *env, jclass clazz, jbyteArray source) {
    (void) clazz;
#if defined(__aarch64__)
    if (source == nullptr) {
        return -1;
    }
    jsize n = env->GetArrayLength(source);
    if (n <= 0) {
        return -1;
    }
    jbyte *bytes = env->GetByteArrayElements(source, nullptr);
    if (bytes == nullptr) {
        return -1;
    }
    int rc = spindle_run_js(reinterpret_cast<const char *>(bytes), n);
    env->ReleaseByteArrayElements(source, bytes, JNI_ABORT);
    return rc;
#else
    (void) env;
    (void) source;
    return -1;
#endif
}

jbyteArray spindleRpc(JNIEnv *env, jclass clazz, jbyteArray json) {
    (void) clazz;
#if defined(__aarch64__)
    if (json == nullptr) {
        return nullptr;
    }
    jsize n = env->GetArrayLength(json);
    jbyte *bytes = env->GetByteArrayElements(json, nullptr);
    if (bytes == nullptr) {
        return nullptr;
    }
    char *out = nullptr;
    spindle_rpc_call(reinterpret_cast<const char *>(bytes), n, &out);
    env->ReleaseByteArrayElements(json, bytes, JNI_ABORT);
    if (out == nullptr) {
        return nullptr;
    }
    jsize len = static_cast<jsize>(strlen(out));
    jbyteArray reply = env->NewByteArray(len);
    if (reply != nullptr && len > 0) {
        env->SetByteArrayRegion(reply, 0, len, reinterpret_cast<const jbyte *>(out));
    }
    free(out);
    return reply;
#else
    (void) json;
    const char *err = "{\"ok\":false,\"e\":\"js runtime is not in bba\"}";
    jsize len = static_cast<jsize>(strlen(err));
    jbyteArray reply = env->NewByteArray(len);
    if (reply != nullptr) {
        env->SetByteArrayRegion(reply, 0, len, reinterpret_cast<const jbyte *>(err));
    }
    return reply;
#endif
}

void spindleHostLog(JNIEnv *env, jclass clazz, jint pid, jstring text) {
    (void) clazz;
    if (text == nullptr) {
        return;
    }
    const char *chars = env->GetStringUTFChars(text, nullptr);
    if (chars == nullptr) {
        return;
    }
    SpindleServer::hostLog(pid, chars);
    env->ReleaseStringUTFChars(text, chars);
}

static JNINativeMethod gMethods[] = {
        {"disableHiddenApi", "()Z",                               (void *) disableHiddenApi},
        {"disableResourceLoading", "()Z",                         (void *) disableResourceLoading},
        {"setupIO",    "(Ljava/util/Map;)V",                      (void *) setupIO},
        {"init",       "(I)V",                                    (void *) init},
        {"gumReplace", "(JJ)J",                                   (void *) gumReplace},
        {"gumRevert", "(J)V",                                     (void *) gumRevert},
        {"gumStalkerFollow", "(JI)Z",                             (void *) gumStalkerFollow},
        {"gumStalkerUnfollow", "(J)V",                            (void *) gumStalkerUnfollow},
        {"gumStalkerExclude", "(JJ)V",                            (void *) gumStalkerExclude},
        {"gumStalkerEventCount", "()I",                           (void *) gumStalkerEventCount},
        {"spindleListen", "()V",                                  (void *) spindleListen},
        {"spindleAdd", "(Ljava/lang/String;II)I",                 (void *) spindleAdd},
        {"spindleDrop", "(I)V",                                   (void *) spindleDrop},
        {"spindleRunJs", "([B)I",                                 (void *) spindleRunJs},
        {"spindleRpc", "([B)[B",                                  (void *) spindleRpc},
        {"spindleHostLog", "(ILjava/lang/String;)V",              (void *) spindleHostLog},
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
    // beer is statically linked; register its JNI natives from this .so.
    jclass sandHook = env->FindClass("com/android/lppi/SandHook");
    jclass neverCall = env->FindClass("com/android/lppi/ClassNeverCall");
    if (sandHook != nullptr && neverCall != nullptr) {
        if (!JNI_Load_Ex(env, sandHook, neverCall)) {
            ALOGE("SandHook JNI_Load_Ex failed");
        }
    } else {
        env->ExceptionClear();
        ALOGE("SandHook classes not found for JNI registration");
    }
    if (sandHook != nullptr) env->DeleteLocalRef(sandHook);
    if (neverCall != nullptr) env->DeleteLocalRef(neverCall);
    return JNI_VERSION_1_6;
}