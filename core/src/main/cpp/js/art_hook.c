#include "art_hook.h"
#include "spindle_str.h"

#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPDL_TAG "SPDL"

#define ART_HOOK_CLASS "com/swift/sandhook/ArtHookBridge"
#define ART_HOOK_HOOK_CLASS "com/swift/sandhook/ArtHookBridge$Hook"

/* "beer" */
static const unsigned char enc_beer[] = {
    0xc5, 0xc2, 0xc2, 0xd5, 0xa7
};
/* "unbeer" */
static const unsigned char enc_unbeer[] = {
    0xd2, 0xc9, 0xc5, 0xc2, 0xc2, 0xd5, 0xa7
};
/* "nativeDispatch" */
static const unsigned char enc_native_dispatch[] = {
    0xc9, 0xc6, 0xd3, 0xce, 0xd1, 0xc2, 0xe3, 0xce, 0xd4, 0xd7, 0xc6, 0xd3, 0xc4,
    0xcf, 0xa7
};
/* "RegisterNatives Beer$Hook" */
static const unsigned char enc_reg_hook[] = {
    0xf5, 0xc2, 0xc0, 0xce, 0xd4, 0xd3, 0xc2, 0xd5, 0xe9, 0xc6, 0xd3, 0xce, 0xd1,
    0xc2, 0xd4, 0x87, 0xe5, 0xc2, 0xc2, 0xd5, 0x83, 0xef, 0xc8, 0xc8, 0xcc, 0xa7
};
/* "Beer method ids" */
static const unsigned char enc_beer_ids[] = {
    0xe5, 0xc2, 0xc2, 0xd5, 0x87, 0xca, 0xc2, 0xd3, 0xcf, 0xc8, 0xc3, 0x87, 0xce,
    0xc3, 0xd4, 0xa7
};
/* "Beer.init" */
static const unsigned char enc_beer_init_log[] = {
    0xe5, 0xc2, 0xc2, 0xd5, 0x89, 0xce, 0xc9, 0xce, 0xd3, 0xa7
};
/* "new Beer.Hook" */
static const unsigned char enc_new_hook[] = {
    0xc9, 0xc2, 0xd0, 0x87, 0xe5, 0xc2, 0xc2, 0xd5, 0x89, 0xef, 0xc8, 0xc8, 0xcc,
    0xa7
};
/* "Beer.beer" */
static const unsigned char enc_beer_beer_log[] = {
    0xe5, 0xc2, 0xc2, 0xd5, 0x89, 0xc5, 0xc2, 0xc2, 0xd5, 0xa7
};
/* "Beer.unbeer" */
static const unsigned char enc_beer_unbeer_log[] = {
    0xe5, 0xc2, 0xc2, 0xd5, 0x89, 0xd2, 0xc9, 0xc5, 0xc2, 0xc2, 0xd5, 0xa7
};

struct art_hook {
    art_hook_enter_cb_t on_enter;
    art_hook_leave_cb_t on_leave;
    art_hook_impl_cb_t impl;
    void* user;
    jobject backup;
    jobject target;
    jobject hook_obj;
    char* class_name;
    char* method_name;
    char* signature;
    int is_static;
    struct art_hook* next;
};

static pthread_mutex_t g_art_mu = PTHREAD_MUTEX_INITIALIZER;
static int g_ready;
static JavaVM* g_vm;
static jclass g_beer_class;
static jclass g_hook_class;
static jclass g_method_class;
static jclass g_object_class;
static jclass g_class_class;
static jmethodID g_beer_init;
static jmethodID g_beer_beer;
static jmethodID g_beer_unbeer;
static jmethodID g_call_origin;
static jmethodID g_hook_ctor;
static jmethodID g_hook_dispatch;
static jmethodID g_method_invoke;
static jmethodID g_object_to_string;
static jmethodID g_class_get_declared_methods;
static jmethodID g_class_get_declared_ctors;
static jmethodID g_class_get_name;
static jmethodID g_class_get_super;
static jmethodID g_method_get_name;
static jmethodID g_method_get_params;
static jmethodID g_method_get_ret;
static jmethodID g_method_get_mods;
static jclass g_ctor_class;
static jmethodID g_ctor_get_params;
static jmethodID g_ctor_new;
static jmethodID g_set_accessible;
static struct art_hook* g_hooks;

static __thread JNIEnv* t_env;
static __thread jobjectArray t_args;
static __thread jobject t_backup;
static __thread jobject t_target;
static __thread int t_called_orig;
static __thread int t_is_static;

static void log_err(const char* s) {
    __android_log_print(ANDROID_LOG_ERROR, SPDL_TAG, "%s", s);
}

static void log_jni_ex(JNIEnv* env, const char* what) {
    jthrowable ex = (*env)->ExceptionOccurred(env);
    if (ex == NULL) {
        return;
    }
    (*env)->ExceptionClear(env);
    if (g_object_to_string != NULL && g_object_class != NULL) {
        jstring msg = (*env)->CallObjectMethod(env, ex, g_object_to_string);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            msg = NULL;
        }
        if (msg != NULL) {
            const char* utf = (*env)->GetStringUTFChars(env, msg, NULL);
            __android_log_print(ANDROID_LOG_ERROR, SPDL_TAG, "%s: %s", what,
                                utf ? utf : "?");
            if (utf) {
                (*env)->ReleaseStringUTFChars(env, msg, utf);
            }
            (*env)->DeleteLocalRef(env, msg);
        } else {
            log_err(what);
        }
    } else {
        log_err(what);
    }
    (*env)->DeleteLocalRef(env, ex);
}

static void log_jni_enc(JNIEnv* env, const unsigned char* enc, size_t n) {
    char what[48];
    sp_dec(what, enc, n);
    log_jni_ex(env, what);
}

extern JavaVM* spindle_java_vm(void);

static JavaVM* find_vm(void) {
    typedef jint (*get_vms_fn)(JavaVM**, jsize, jsize*);
    get_vms_fn fn;
    JavaVM* vm = NULL;
    jsize n = 0;
    vm = spindle_java_vm();
    if (vm != NULL) {
        return vm;
    }
    fn = (get_vms_fn)dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs");
    if (fn == NULL) {
        return NULL;
    }
    if (fn(&vm, 1, &n) != JNI_OK || n < 1) {
        return NULL;
    }
    return vm;
}

static JNIEnv* get_env(void) {
    JNIEnv* env = NULL;
    jint rc;
    if (g_vm == NULL) {
        return NULL;
    }
    rc = (*g_vm)->GetEnv(g_vm, (void**)&env, JNI_VERSION_1_6);
    if (rc == JNI_OK) {
        return env;
    }
    if ((*g_vm)->AttachCurrentThread(g_vm, &env, NULL) != JNI_OK) {
        return NULL;
    }
    return env;
}

static char* xstrdup(const char* s) {
    size_t n;
    char* p;
    if (s == NULL) {
        s = "";
    }
    n = strlen(s) + 1;
    p = malloc(n);
    if (p == NULL) {
        return NULL;
    }
    memcpy(p, s, n);
    return p;
}

static void dots_to_slashes(char* s) {
    for (; *s; s++) {
        if (*s == '.') {
            *s = '/';
        }
    }
}

static void slashes_to_dots(char* s) {
    for (; *s; s++) {
        if (*s == '/') {
            *s = '.';
        }
    }
}

static int same_cstr(const char* a, const char* b) {
    if (a == NULL) {
        a = "";
    }
    if (b == NULL) {
        b = "";
    }
    return strcmp(a, b) == 0;
}

static jobject invoke_backup(JNIEnv* env, jobject target, jobjectArray args,
                             int is_static) {
    jobject ret;
    jobject receiver = NULL;
    jobjectArray invoke_args = args;
    jsize n;
    jsize i;
    if (target == NULL) {
        return NULL;
    }
    /* Beer packs [this, ...params] for instance methods. Do not trust
     * backup.getModifiers() — the reflected Method may still look static. */
    if (!is_static) {
        n = args != NULL ? (*env)->GetArrayLength(env, args) : 0;
        if (n < 1) {
            log_err("instance backup missing this");
            return NULL;
        }
        receiver = (*env)->GetObjectArrayElement(env, args, 0);
        if (receiver == NULL) {
            log_err("instance backup null this");
            return NULL;
        }
        invoke_args = (*env)->NewObjectArray(env, n - 1, g_object_class, NULL);
        if (invoke_args == NULL) {
            (*env)->DeleteLocalRef(env, receiver);
            return NULL;
        }
        for (i = 1; i < n; i++) {
            jobject o = (*env)->GetObjectArrayElement(env, args, i);
            (*env)->SetObjectArrayElement(env, invoke_args, i - 1, o);
            if (o != NULL) {
                (*env)->DeleteLocalRef(env, o);
            }
        }
    }
    ret = (*env)->CallStaticObjectMethod(env, g_beer_class, g_call_origin, target,
                                            receiver, invoke_args);
    if (receiver != NULL) {
        (*env)->DeleteLocalRef(env, receiver);
    }
    if (invoke_args != args && invoke_args != NULL) {
        (*env)->DeleteLocalRef(env, invoke_args);
    }
    if ((*env)->ExceptionCheck(env)) {
        log_jni_ex(env, "backup.invoke");
        return NULL;
    }
    return ret;
}

static jobject JNICALL native_dispatch(JNIEnv* env, jclass clazz, jlong cookie,
                                       jobjectArray args) {
    struct art_hook* hook = (struct art_hook*)(uintptr_t)cookie;
    jobject ret = NULL;
    JNIEnv* old_env = t_env;
    jobjectArray old_args = t_args;
    jobject old_backup = t_backup;
    jobject old_target = t_target;
    int old_called = t_called_orig;
    int old_static = t_is_static;
    (void)clazz;
    if (hook == NULL) {
        return NULL;
    }
    t_env = env;
    t_args = args;
    t_backup = hook->backup;
    t_target = hook->target;
    t_called_orig = 0;
    t_is_static = hook->is_static;
    if (hook->on_enter != NULL) {
        hook->on_enter(hook->user, env, args);
    }
    if (hook->impl != NULL) {
        ret = hook->impl(hook->user, env, args);
    } else if (!t_called_orig) {
        ret = invoke_backup(env, hook->target, args, hook->is_static);
    }
    if (hook->on_leave != NULL && !(*env)->ExceptionCheck(env)) {
        jobject next = hook->on_leave(hook->user, env, ret);
        ret = next;
    }
    t_env = old_env;
    t_args = old_args;
    t_backup = old_backup;
    t_target = old_target;
    t_called_orig = old_called;
    t_is_static = old_static;
    return ret;
}

static int register_hook_natives(JNIEnv* env) {
    JNINativeMethod methods[1];
    char name[sizeof(enc_native_dispatch)];
    char what[sizeof(enc_reg_hook)];
    sp_dec(name, enc_native_dispatch, sizeof(enc_native_dispatch));
    methods[0].name = name;
    methods[0].signature = "(J[Ljava/lang/Object;)Ljava/lang/Object;";
    methods[0].fnPtr = (void*)native_dispatch;
    if ((*env)->RegisterNatives(env, g_hook_class, methods, 1) != 0) {
        sp_dec(what, enc_reg_hook, sizeof(enc_reg_hook));
        log_jni_ex(env, what);
        return -1;
    }
    return 0;
}

static jclass global_class(JNIEnv* env, const char* name) {
    jclass local = (*env)->FindClass(env, name);
    jclass global;
    if (local == NULL) {
        log_jni_ex(env, name);
        return NULL;
    }
    global = (*env)->NewGlobalRef(env, local);
    (*env)->DeleteLocalRef(env, local);
    return global;
}

static jobject load_app_class(JNIEnv* env, const char* dot_name) {
    jclass at;
    jmethodID current_app;
    jobject app;
    jclass ctx;
    jmethodID get_cl;
    jobject cl;
    jclass clc;
    jmethodID load;
    jstring jname;
    jobject klass;
    at = (*env)->FindClass(env, "android/app/ActivityThread");
    if (at == NULL) {
        (*env)->ExceptionClear(env);
        return NULL;
    }
    current_app = (*env)->GetStaticMethodID(env, at, "currentApplication",
                                            "()Landroid/app/Application;");
    if (current_app == NULL) {
        (*env)->ExceptionClear(env);
        (*env)->DeleteLocalRef(env, at);
        return NULL;
    }
    app = (*env)->CallStaticObjectMethod(env, at, current_app);
    (*env)->DeleteLocalRef(env, at);
    if ((*env)->ExceptionCheck(env) || app == NULL) {
        (*env)->ExceptionClear(env);
        return NULL;
    }
    ctx = (*env)->FindClass(env, "android/content/Context");
    if (ctx == NULL) {
        (*env)->ExceptionClear(env);
        (*env)->DeleteLocalRef(env, app);
        return NULL;
    }
    get_cl = (*env)->GetMethodID(env, ctx, "getClassLoader",
                                 "()Ljava/lang/ClassLoader;");
    (*env)->DeleteLocalRef(env, ctx);
    if (get_cl == NULL) {
        (*env)->ExceptionClear(env);
        (*env)->DeleteLocalRef(env, app);
        return NULL;
    }
    cl = (*env)->CallObjectMethod(env, app, get_cl);
    (*env)->DeleteLocalRef(env, app);
    if ((*env)->ExceptionCheck(env) || cl == NULL) {
        (*env)->ExceptionClear(env);
        return NULL;
    }
    clc = (*env)->FindClass(env, "java/lang/ClassLoader");
    if (clc == NULL) {
        (*env)->ExceptionClear(env);
        (*env)->DeleteLocalRef(env, cl);
        return NULL;
    }
    load = (*env)->GetMethodID(env, clc, "loadClass",
                               "(Ljava/lang/String;)Ljava/lang/Class;");
    (*env)->DeleteLocalRef(env, clc);
    if (load == NULL) {
        (*env)->ExceptionClear(env);
        (*env)->DeleteLocalRef(env, cl);
        return NULL;
    }
    jname = (*env)->NewStringUTF(env, dot_name);
    klass = (*env)->CallObjectMethod(env, cl, load, jname);
    (*env)->DeleteLocalRef(env, cl);
    (*env)->DeleteLocalRef(env, jname);
    if ((*env)->ExceptionCheck(env)) {
        log_jni_ex(env, "loadClass");
        return NULL;
    }
    return klass;
}

static jclass find_target_class(JNIEnv* env, const char* class_name) {
    char* slash;
    char* dot;
    jclass clazz;
    slash = xstrdup(class_name);
    dot = xstrdup(class_name);
    if (slash == NULL || dot == NULL) {
        free(slash);
        free(dot);
        return NULL;
    }
    dots_to_slashes(slash);
    slashes_to_dots(dot);
    clazz = (*env)->FindClass(env, slash);
    if (clazz == NULL) {
        (*env)->ExceptionClear(env);
        clazz = (jclass)load_app_class(env, dot);
    }
    free(slash);
    free(dot);
    return clazz;
}

static jobject unique_declared(JNIEnv* env, jclass clazz, const char* name) {
    jobjectArray methods;
    jobjectArray ctors;
    jobject found = NULL;
    jsize i;
    jsize n;
    int hits = 0;
    int want_ctor = strcmp(name, "<init>") == 0;
    if (want_ctor) {
        ctors = (*env)->CallObjectMethod(env, clazz, g_class_get_declared_ctors);
        if ((*env)->ExceptionCheck(env) || ctors == NULL) {
            log_jni_ex(env, "getDeclaredConstructors");
            return NULL;
        }
        n = (*env)->GetArrayLength(env, ctors);
        if (n == 1) {
            found = (*env)->NewGlobalRef(env, (*env)->GetObjectArrayElement(env, ctors, 0));
        }
        (*env)->DeleteLocalRef(env, ctors);
        if (n != 1) {
            log_err("ambiguous or missing constructor, pass a signature");
            return NULL;
        }
        return found;
    }
    methods = (*env)->CallObjectMethod(env, clazz, g_class_get_declared_methods);
    if ((*env)->ExceptionCheck(env) || methods == NULL) {
        log_jni_ex(env, "getDeclaredMethods");
        return NULL;
    }
    n = (*env)->GetArrayLength(env, methods);
    for (i = 0; i < n; i++) {
        jobject m = (*env)->GetObjectArrayElement(env, methods, i);
        jstring jn = (*env)->CallObjectMethod(env, m, g_method_get_name);
        const char* utf;
        int match = 0;
        if (jn == NULL) {
            (*env)->DeleteLocalRef(env, m);
            continue;
        }
        utf = (*env)->GetStringUTFChars(env, jn, NULL);
        match = utf != NULL && strcmp(utf, name) == 0;
        if (utf) {
            (*env)->ReleaseStringUTFChars(env, jn, utf);
        }
        (*env)->DeleteLocalRef(env, jn);
        if (match) {
            hits++;
            if (found != NULL) {
                (*env)->DeleteGlobalRef(env, found);
            }
            found = (*env)->NewGlobalRef(env, m);
        }
        (*env)->DeleteLocalRef(env, m);
    }
    (*env)->DeleteLocalRef(env, methods);
    if (hits != 1) {
        if (found != NULL) {
            (*env)->DeleteGlobalRef(env, found);
        }
        log_err("ambiguous or missing method, pass a signature");
        return NULL;
    }
    return found;
}

static jobject resolve_method(JNIEnv* env, jclass clazz, const char* name,
                              const char* signature) {
    jmethodID mid;
    jobject reflected;
    if (signature == NULL || signature[0] == '\0') {
        return unique_declared(env, clazz, name);
    }
    mid = (*env)->GetMethodID(env, clazz, name, signature);
    if (mid != NULL && !(*env)->ExceptionCheck(env)) {
        reflected = (*env)->ToReflectedMethod(env, clazz, mid, JNI_FALSE);
        if (reflected != NULL) {
            jobject global = (*env)->NewGlobalRef(env, reflected);
            (*env)->DeleteLocalRef(env, reflected);
            return global;
        }
    }
    (*env)->ExceptionClear(env);
    mid = (*env)->GetStaticMethodID(env, clazz, name, signature);
    if (mid == NULL || (*env)->ExceptionCheck(env)) {
        log_jni_ex(env, "GetMethodID");
        return NULL;
    }
    reflected = (*env)->ToReflectedMethod(env, clazz, mid, JNI_TRUE);
    if (reflected == NULL) {
        log_jni_ex(env, "ToReflectedMethod");
        return NULL;
    }
    {
        jobject global = (*env)->NewGlobalRef(env, reflected);
        (*env)->DeleteLocalRef(env, reflected);
        return global;
    }
}

static void free_hook(JNIEnv* env, struct art_hook* hook) {
    if (hook == NULL) {
        return;
    }
    if (env != NULL) {
        if (hook->backup != NULL) {
            (*env)->DeleteGlobalRef(env, hook->backup);
        }
        if (hook->target != NULL) {
            (*env)->DeleteGlobalRef(env, hook->target);
        }
        if (hook->hook_obj != NULL) {
            (*env)->DeleteGlobalRef(env, hook->hook_obj);
        }
    }
    free(hook->class_name);
    free(hook->method_name);
    free(hook->signature);
    free(hook);
}

static int unlink_hook(struct art_hook* hook) {
    struct art_hook** pp = &g_hooks;
    while (*pp != NULL) {
        if (*pp == hook) {
            *pp = hook->next;
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

static struct art_hook* find_hook(const char* class_name, const char* method_name,
                                  const char* signature) {
    struct art_hook* h;
    for (h = g_hooks; h != NULL; h = h->next) {
        if (same_cstr(h->class_name, class_name) &&
            same_cstr(h->method_name, method_name) &&
            same_cstr(h->signature, signature)) {
            return h;
        }
    }
    return NULL;
}

static int ensure_locked(JNIEnv* env) {
    jboolean ok;
    if (g_ready) {
        return 0;
    }
    g_beer_class = global_class(env, ART_HOOK_CLASS);
    g_hook_class = global_class(env, ART_HOOK_HOOK_CLASS);
    g_method_class = global_class(env, "java/lang/reflect/Method");
    g_object_class = global_class(env, "java/lang/Object");
    g_class_class = global_class(env, "java/lang/Class");
    if (g_beer_class == NULL || g_hook_class == NULL || g_method_class == NULL ||
        g_object_class == NULL || g_class_class == NULL) {
        return -1;
    }
    g_beer_init = (*env)->GetStaticMethodID(env, g_beer_class, "init", "()Z");
    {
        char beer[sizeof(enc_beer)];
        char unbeer[sizeof(enc_unbeer)];
        sp_dec(beer, enc_beer, sizeof(enc_beer));
        sp_dec(unbeer, enc_unbeer, sizeof(enc_unbeer));
        g_beer_beer = (*env)->GetStaticMethodID(
            env, g_beer_class, beer,
            "(Ljava/lang/Object;Ljava/lang/reflect/Member;Ljava/lang/reflect/Method;)"
            "Ljava/lang/reflect/Method;");
        g_beer_unbeer = (*env)->GetStaticMethodID(env, g_beer_class, unbeer,
                                                  "(Ljava/lang/reflect/Member;)Z");
        g_call_origin = (*env)->GetStaticMethodID(
            env, g_beer_class, "callOrigin",
            "(Ljava/lang/reflect/Member;Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
    }
    g_hook_ctor = (*env)->GetMethodID(env, g_hook_class, "<init>", "(J)V");
    g_hook_dispatch = (*env)->GetMethodID(env, g_hook_class, "dispatch",
                                          "([Ljava/lang/Object;)Ljava/lang/Object;");
    g_method_invoke = (*env)->GetMethodID(
        env, g_method_class, "invoke",
        "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;");
    g_object_to_string =
        (*env)->GetMethodID(env, g_object_class, "toString", "()Ljava/lang/String;");
    g_class_get_declared_methods = (*env)->GetMethodID(
        env, g_class_class, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;");
    g_class_get_declared_ctors = (*env)->GetMethodID(
        env, g_class_class, "getDeclaredConstructors",
        "()[Ljava/lang/reflect/Constructor;");
    g_method_get_name =
        (*env)->GetMethodID(env, g_method_class, "getName", "()Ljava/lang/String;");
    g_class_get_name =
        (*env)->GetMethodID(env, g_class_class, "getName", "()Ljava/lang/String;");
    g_class_get_super =
        (*env)->GetMethodID(env, g_class_class, "getSuperclass", "()Ljava/lang/Class;");
    g_method_get_params = (*env)->GetMethodID(env, g_method_class, "getParameterTypes",
                                              "()[Ljava/lang/Class;");
    g_method_get_ret = (*env)->GetMethodID(env, g_method_class, "getReturnType",
                                           "()Ljava/lang/Class;");
    g_method_get_mods = (*env)->GetMethodID(env, g_method_class, "getModifiers", "()I");
    g_ctor_class = global_class(env, "java/lang/reflect/Constructor");
    g_ctor_get_params = g_ctor_class == NULL
                            ? NULL
                            : (*env)->GetMethodID(env, g_ctor_class, "getParameterTypes",
                                                  "()[Ljava/lang/Class;");
    g_ctor_new = g_ctor_class == NULL
                     ? NULL
                     : (*env)->GetMethodID(env, g_ctor_class, "newInstance",
                                           "([Ljava/lang/Object;)Ljava/lang/Object;");
    {
        jclass acc = (*env)->FindClass(env, "java/lang/reflect/AccessibleObject");
        if (acc != NULL) {
            g_set_accessible = (*env)->GetMethodID(env, acc, "setAccessible", "(Z)V");
            (*env)->DeleteLocalRef(env, acc);
        }
    }
    if (g_beer_init == NULL || g_beer_beer == NULL || g_beer_unbeer == NULL ||
        g_call_origin == NULL ||
        g_hook_ctor == NULL || g_hook_dispatch == NULL || g_method_invoke == NULL ||
        g_object_to_string == NULL || g_class_get_declared_methods == NULL ||
        g_class_get_declared_ctors == NULL || g_method_get_name == NULL ||
        g_class_get_name == NULL || g_class_get_super == NULL ||
        g_method_get_params == NULL || g_method_get_ret == NULL ||
        g_method_get_mods == NULL || g_ctor_class == NULL ||
        g_ctor_get_params == NULL || g_ctor_new == NULL ||
        g_set_accessible == NULL) {
        log_jni_enc(env, enc_beer_ids, sizeof(enc_beer_ids));
        return -1;
    }
    if (register_hook_natives(env) != 0) {
        return -1;
    }
    ok = (*env)->CallStaticBooleanMethod(env, g_beer_class, g_beer_init);
    if ((*env)->ExceptionCheck(env) || !ok) {
        log_jni_enc(env, enc_beer_init_log, sizeof(enc_beer_init_log));
        return -1;
    }
    g_ready = 1;
    return 0;
}

int art_hook_init(void) {
    JNIEnv* env;
    int rc;
    pthread_mutex_lock(&g_art_mu);
    if (g_ready) {
        pthread_mutex_unlock(&g_art_mu);
        return 0;
    }
    g_vm = find_vm();
    env = get_env();
    if (env == NULL) {
        pthread_mutex_unlock(&g_art_mu);
        log_err("no JavaVM");
        return -1;
    }
    rc = ensure_locked(env);
    pthread_mutex_unlock(&g_art_mu);
    return rc;
}

int art_hook_attach(const char* class_name, const char* method_name,
                    const char* signature, art_hook_enter_cb_t on_enter,
                    art_hook_leave_cb_t on_leave, art_hook_impl_cb_t impl,
                    void* ctx) {
    JNIEnv* env;
    jclass clazz = NULL;
    jobject target = NULL;
    jobject hook_obj = NULL;
    jobject callback = NULL;
    jobject backup = NULL;
    struct art_hook* hook = NULL;
    int rc = -1;

    if (class_name == NULL || method_name == NULL) {
        return -1;
    }
    if (on_enter == NULL && on_leave == NULL && impl == NULL) {
        return -1;
    }
    if (art_hook_init() != 0) {
        return -1;
    }
    env = get_env();
    if (env == NULL) {
        return -1;
    }

    pthread_mutex_lock(&g_art_mu);
    if (find_hook(class_name, method_name, signature) != NULL) {
        log_err("already hooked");
        goto out;
    }
    clazz = find_target_class(env, class_name);
    if (clazz == NULL) {
        log_err("class not found");
        goto out;
    }
    target = resolve_method(env, clazz, method_name, signature);
    if (target == NULL) {
        goto out;
    }
    hook = calloc(1, sizeof(*hook));
    if (hook == NULL) {
        goto out;
    }
    hook->on_enter = on_enter;
    hook->on_leave = on_leave;
    hook->impl = impl;
    hook->user = ctx;
    /* Use the original Method flags — Beer backup's reflected Method may
     * still report static even after CopyFrom. */
    {
        jint mods = (*env)->CallIntMethod(env, target, g_method_get_mods);
        if ((*env)->ExceptionCheck(env)) {
            log_jni_ex(env, "getModifiers");
            goto out;
        }
        hook->is_static = (mods & 0x8) != 0;
    }
    hook->class_name = xstrdup(class_name);
    hook->method_name = xstrdup(method_name);
    hook->signature = xstrdup(signature ? signature : "");
    if (hook->class_name == NULL || hook->method_name == NULL ||
        hook->signature == NULL) {
        goto out;
    }
    hook_obj = (*env)->NewObject(env, g_hook_class, g_hook_ctor,
                                 (jlong)(uintptr_t)hook);
    if (hook_obj == NULL || (*env)->ExceptionCheck(env)) {
        log_jni_enc(env, enc_new_hook, sizeof(enc_new_hook));
        goto out;
    }
    hook->hook_obj = (*env)->NewGlobalRef(env, hook_obj);
    callback = (*env)->ToReflectedMethod(env, g_hook_class, g_hook_dispatch,
                                         JNI_FALSE);
    if (callback == NULL || (*env)->ExceptionCheck(env)) {
        log_jni_ex(env, "Hook.dispatch");
        goto out;
    }
    backup = (*env)->CallStaticObjectMethod(env, g_beer_class, g_beer_beer,
                                            hook->hook_obj, target, callback);
    if (backup == NULL || (*env)->ExceptionCheck(env)) {
        log_jni_enc(env, enc_beer_beer_log, sizeof(enc_beer_beer_log));
        goto out;
    }
    hook->backup = (*env)->NewGlobalRef(env, backup);
    hook->target = target;
    target = NULL;
    hook->next = g_hooks;
    g_hooks = hook;
    hook = NULL;
    rc = 0;
out:
    if (clazz != NULL) {
        (*env)->DeleteLocalRef(env, clazz);
    }
    if (hook_obj != NULL) {
        (*env)->DeleteLocalRef(env, hook_obj);
    }
    if (callback != NULL) {
        (*env)->DeleteLocalRef(env, callback);
    }
    if (backup != NULL) {
        (*env)->DeleteLocalRef(env, backup);
    }
    if (target != NULL) {
        (*env)->DeleteGlobalRef(env, target);
    }
    if (hook != NULL) {
        free_hook(env, hook);
    }
    pthread_mutex_unlock(&g_art_mu);
    return rc;
}

int art_hook_detach(const char* class_name, const char* method_name,
                    const char* signature) {
    JNIEnv* env;
    struct art_hook* hook;
    jboolean ok;
    if (class_name == NULL || method_name == NULL) {
        return -1;
    }
    if (art_hook_init() != 0) {
        return -1;
    }
    env = get_env();
    if (env == NULL) {
        return -1;
    }
    pthread_mutex_lock(&g_art_mu);
    hook = find_hook(class_name, method_name, signature);
    if (hook == NULL) {
        pthread_mutex_unlock(&g_art_mu);
        return -1;
    }
    ok = (*env)->CallStaticBooleanMethod(env, g_beer_class, g_beer_unbeer,
                                         hook->target);
    if ((*env)->ExceptionCheck(env) || !ok) {
        log_jni_enc(env, enc_beer_unbeer_log, sizeof(enc_beer_unbeer_log));
        pthread_mutex_unlock(&g_art_mu);
        return -1;
    }
    unlink_hook(hook);
    free_hook(env, hook);
    pthread_mutex_unlock(&g_art_mu);
    return 0;
}

void* art_hook_call_original(void) {
    if (t_env == NULL || t_target == NULL) {
        return NULL;
    }
    t_called_orig = 1;
    return invoke_backup(t_env, t_target, t_args, t_is_static);
}

void* art_hook_current_env(void) {
    return t_env;
}

void* art_hook_jni_env(void) {
    if (art_hook_init() != 0) {
        return NULL;
    }
    return t_env != NULL ? t_env : get_env();
}

int art_hook_to_string(void* jobj, char* buf, size_t len) {
    JNIEnv* env;
    jstring s;
    const char* utf;
    if (buf == NULL || len == 0) {
        return -1;
    }
    buf[0] = '\0';
    if (jobj == NULL) {
        strncpy(buf, "null", len - 1);
        buf[len - 1] = '\0';
        return 0;
    }
    env = t_env != NULL ? t_env : get_env();
    if (env == NULL || g_object_to_string == NULL) {
        return -1;
    }
    s = (*env)->CallObjectMethod(env, (jobject)jobj, g_object_to_string);
    if ((*env)->ExceptionCheck(env) || s == NULL) {
        log_jni_ex(env, "toString");
        return -1;
    }
    utf = (*env)->GetStringUTFChars(env, s, NULL);
    if (utf == NULL) {
        (*env)->DeleteLocalRef(env, s);
        return -1;
    }
    strncpy(buf, utf, len - 1);
    buf[len - 1] = '\0';
    (*env)->ReleaseStringUTFChars(env, s, utf);
    (*env)->DeleteLocalRef(env, s);
    return 0;
}

static int copy_jstring(JNIEnv* env, jstring s, char* buf, size_t cap) {
    const char* utf;
    if (s == NULL || buf == NULL || cap == 0) {
        return -1;
    }
    utf = (*env)->GetStringUTFChars(env, s, NULL);
    if (utf == NULL) {
        return -1;
    }
    strncpy(buf, utf, cap - 1);
    buf[cap - 1] = '\0';
    (*env)->ReleaseStringUTFChars(env, s, utf);
    return 0;
}

int art_hook_class_name(void* jobj, char* buf, size_t len) {
    JNIEnv* env;
    jclass clazz;
    jstring name;
    int rc;
    if (buf == NULL || len == 0) {
        return -1;
    }
    buf[0] = '\0';
    if (jobj == NULL) {
        return -1;
    }
    if (art_hook_init() != 0) {
        return -1;
    }
    env = t_env != NULL ? t_env : get_env();
    if (env == NULL || g_class_get_name == NULL) {
        return -1;
    }
    clazz = (*env)->GetObjectClass(env, (jobject)jobj);
    if (clazz == NULL) {
        return -1;
    }
    name = (*env)->CallObjectMethod(env, clazz, g_class_get_name);
    (*env)->DeleteLocalRef(env, clazz);
    if ((*env)->ExceptionCheck(env) || name == NULL) {
        log_jni_ex(env, "Class.getName");
        return -1;
    }
    rc = copy_jstring(env, name, buf, len);
    (*env)->DeleteLocalRef(env, name);
    return rc;
}

static int append_str(char* buf, size_t cap, size_t* n, const char* s) {
    size_t len = strlen(s);
    if (*n + len + 1 > cap) {
        return -1;
    }
    memcpy(buf + *n, s, len + 1);
    *n += len;
    return 0;
}

static int class_java_name(JNIEnv* env, jclass c, char* buf, size_t cap) {
    jstring name = (*env)->CallObjectMethod(env, c, g_class_get_name);
    int rc;
    if ((*env)->ExceptionCheck(env) || name == NULL) {
        log_jni_ex(env, "type getName");
        return -1;
    }
    rc = copy_jstring(env, name, buf, cap);
    (*env)->DeleteLocalRef(env, name);
    return rc;
}

static const char* prim_jni(const char* name) {
    if (strcmp(name, "void") == 0) {
        return "V";
    }
    if (strcmp(name, "boolean") == 0) {
        return "Z";
    }
    if (strcmp(name, "byte") == 0) {
        return "B";
    }
    if (strcmp(name, "char") == 0) {
        return "C";
    }
    if (strcmp(name, "short") == 0) {
        return "S";
    }
    if (strcmp(name, "int") == 0) {
        return "I";
    }
    if (strcmp(name, "long") == 0) {
        return "J";
    }
    if (strcmp(name, "float") == 0) {
        return "F";
    }
    if (strcmp(name, "double") == 0) {
        return "D";
    }
    return NULL;
}

static int class_jni(JNIEnv* env, jclass c, char* buf, size_t cap, size_t* n) {
    char name[256];
    const char* prim;
    char tmp[320];
    size_t i;
    if (class_java_name(env, c, name, sizeof(name)) != 0) {
        return -1;
    }
    prim = prim_jni(name);
    if (prim != NULL) {
        return append_str(buf, cap, n, prim);
    }
    if (name[0] == '[') {
        for (i = 0; name[i]; i++) {
            if (name[i] == '.') {
                name[i] = '/';
            }
        }
        return append_str(buf, cap, n, name);
    }
    if (snprintf(tmp, sizeof(tmp), "L%s;", name) >= (int)sizeof(tmp)) {
        return -1;
    }
    for (i = 0; tmp[i]; i++) {
        if (tmp[i] == '.') {
            tmp[i] = '/';
        }
    }
    return append_str(buf, cap, n, tmp);
}

static int already_listed(const struct art_method_info* list, int count,
                          const char* name, const char* sig) {
    int i;
    for (i = 0; i < count; i++) {
        if (strcmp(list[i].name, name) == 0 && strcmp(list[i].sig, sig) == 0) {
            return 1;
        }
    }
    return 0;
}

static int fill_params(JNIEnv* env, jobjectArray types, struct art_method_info* info,
                       char* sig, size_t sig_cap, size_t* sig_n) {
    jsize n = types == NULL ? 0 : (*env)->GetArrayLength(env, types);
    jsize i;
    info->argc = (int)n;
    if (n > ART_METHOD_MAX_PARAMS) {
        return -1;
    }
    if (append_str(sig, sig_cap, sig_n, "(") != 0) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        jclass t = (*env)->GetObjectArrayElement(env, types, i);
        if (t == NULL || class_java_name(env, t, info->params[i],
                                         sizeof(info->params[i])) != 0 ||
            class_jni(env, t, sig, sig_cap, sig_n) != 0) {
            if (t != NULL) {
                (*env)->DeleteLocalRef(env, t);
            }
            return -1;
        }
        (*env)->DeleteLocalRef(env, t);
    }
    return append_str(sig, sig_cap, sig_n, ")");
}

static int add_method(JNIEnv* env, jobject method, int is_ctor,
                      struct art_method_info** list, int* count, int* cap) {
    struct art_method_info info;
    jobjectArray types;
    jclass ret_cl = NULL;
    char sig[512];
    size_t sig_n = 0;
    jint mods;
    memset(&info, 0, sizeof(info));
    sig[0] = '\0';
    if (is_ctor) {
        strncpy(info.name, "<init>", sizeof(info.name) - 1);
        types = (*env)->CallObjectMethod(env, method, g_ctor_get_params);
        info.is_ctor = 1;
        info.is_static = 0;
    } else {
        jstring jn = (*env)->CallObjectMethod(env, method, g_method_get_name);
        if ((*env)->ExceptionCheck(env) || jn == NULL ||
            copy_jstring(env, jn, info.name, sizeof(info.name)) != 0) {
            if (jn != NULL) {
                (*env)->DeleteLocalRef(env, jn);
            }
            (*env)->ExceptionClear(env);
            return 0;
        }
        (*env)->DeleteLocalRef(env, jn);
        if (info.name[0] == '<') {
            return 0;
        }
        types = (*env)->CallObjectMethod(env, method, g_method_get_params);
        mods = (*env)->CallIntMethod(env, method, g_method_get_mods);
        info.is_static = (mods & 0x8) != 0;
    }
    if ((*env)->ExceptionCheck(env) || types == NULL) {
        log_jni_ex(env, "getParameterTypes");
        return -1;
    }
    if (fill_params(env, types, &info, sig, sizeof(sig), &sig_n) != 0) {
        (*env)->DeleteLocalRef(env, types);
        return 0;
    }
    (*env)->DeleteLocalRef(env, types);
    if (is_ctor) {
        if (append_str(sig, sizeof(sig), &sig_n, "V") != 0) {
            return 0;
        }
    } else {
        ret_cl = (*env)->CallObjectMethod(env, method, g_method_get_ret);
        if ((*env)->ExceptionCheck(env) || ret_cl == NULL ||
            class_jni(env, ret_cl, sig, sizeof(sig), &sig_n) != 0) {
            if (ret_cl != NULL) {
                (*env)->DeleteLocalRef(env, ret_cl);
            }
            (*env)->ExceptionClear(env);
            return 0;
        }
        (*env)->DeleteLocalRef(env, ret_cl);
    }
    strncpy(info.sig, sig, sizeof(info.sig) - 1);
    if (already_listed(*list, *count, info.name, info.sig)) {
        return 0;
    }
    if (*count >= *cap) {
        int ncap = *cap == 0 ? 64 : *cap * 2;
        struct art_method_info* nl = realloc(*list, (size_t)ncap * sizeof(*nl));
        if (nl == NULL) {
            return -1;
        }
        *list = nl;
        *cap = ncap;
    }
    (*list)[(*count)++] = info;
    return 0;
}

int art_hook_list_methods(const char* class_name, struct art_method_info** out,
                          int* count) {
    JNIEnv* env;
    jclass clazz;
    jclass walk;
    struct art_method_info* list = NULL;
    int n = 0;
    int cap = 0;
    int first = 1;
    if (out == NULL || count == NULL || class_name == NULL) {
        return -1;
    }
    *out = NULL;
    *count = 0;
    if (art_hook_init() != 0) {
        return -1;
    }
    env = get_env();
    if (env == NULL) {
        return -1;
    }
    clazz = find_target_class(env, class_name);
    if (clazz == NULL) {
        log_err("class not found");
        return -1;
    }
    walk = clazz;
    while (walk != NULL) {
        jobjectArray methods = (*env)->CallObjectMethod(env, walk,
                                                        g_class_get_declared_methods);
        jsize i;
        jsize m;
        if ((*env)->ExceptionCheck(env) || methods == NULL) {
            log_jni_ex(env, "getDeclaredMethods");
            if (walk != clazz) {
                (*env)->DeleteLocalRef(env, walk);
            }
            (*env)->DeleteLocalRef(env, clazz);
            art_hook_list_free(list);
            return -1;
        }
        m = (*env)->GetArrayLength(env, methods);
        for (i = 0; i < m; i++) {
            jobject method = (*env)->GetObjectArrayElement(env, methods, i);
            if (method != NULL) {
                add_method(env, method, 0, &list, &n, &cap);
                (*env)->DeleteLocalRef(env, method);
            }
        }
        (*env)->DeleteLocalRef(env, methods);
        if (first) {
            jobjectArray ctors = (*env)->CallObjectMethod(env, walk,
                                                          g_class_get_declared_ctors);
            if ((*env)->ExceptionCheck(env)) {
                log_jni_ex(env, "getDeclaredConstructors");
            } else if (ctors != NULL) {
                m = (*env)->GetArrayLength(env, ctors);
                for (i = 0; i < m; i++) {
                    jobject ctor = (*env)->GetObjectArrayElement(env, ctors, i);
                    if (ctor != NULL) {
                        add_method(env, ctor, 1, &list, &n, &cap);
                        (*env)->DeleteLocalRef(env, ctor);
                    }
                }
                (*env)->DeleteLocalRef(env, ctors);
            }
            first = 0;
        }
        {
            jclass super = (*env)->CallObjectMethod(env, walk, g_class_get_super);
            if (walk != clazz) {
                (*env)->DeleteLocalRef(env, walk);
            }
            walk = super;
        }
    }
    (*env)->DeleteLocalRef(env, clazz);
    *out = list;
    *count = n;
    return 0;
}

void art_hook_list_free(struct art_method_info* list) {
    free(list);
}

static jobjectArray prepend_this(JNIEnv* env, jobject thiz, jobjectArray params) {
    jsize n = params == NULL ? 0 : (*env)->GetArrayLength(env, params);
    jobjectArray out = (*env)->NewObjectArray(env, n + 1, g_object_class, NULL);
    jsize i;
    if (out == NULL) {
        return NULL;
    }
    (*env)->SetObjectArrayElement(env, out, 0, thiz);
    for (i = 0; i < n; i++) {
        jobject o = (*env)->GetObjectArrayElement(env, params, i);
        (*env)->SetObjectArrayElement(env, out, i + 1, o);
        if (o != NULL) {
            (*env)->DeleteLocalRef(env, o);
        }
    }
    return out;
}

static void make_accessible(JNIEnv* env, jobject member) {
    if (member == NULL || g_set_accessible == NULL) {
        return;
    }
    (*env)->CallVoidMethod(env, member, g_set_accessible, JNI_TRUE);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
}

void* art_hook_invoke(const char* class_name, const char* method_name,
                      const char* signature, void* this_jobj, void* params,
                      int is_static) {
    JNIEnv* env;
    struct art_hook* hook;
    jobjectArray parr = (jobjectArray)params;
    jobject ret;
    jobject boxed = NULL;
    jobject target;
    jclass clazz;
    if (class_name == NULL || method_name == NULL) {
        return NULL;
    }
    if (art_hook_init() != 0) {
        return NULL;
    }
    env = t_env != NULL ? t_env : get_env();
    if (env == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&g_art_mu);
    hook = find_hook(class_name, method_name, signature);
    pthread_mutex_unlock(&g_art_mu);
    if (hook != NULL && hook->backup != NULL) {
        if (!is_static) {
            boxed = prepend_this(env, (jobject)this_jobj, parr);
            ret = invoke_backup(env, hook->target, boxed, 0);
            if (boxed != NULL) {
                (*env)->DeleteLocalRef(env, boxed);
            }
            return ret;
        }
        return invoke_backup(env, hook->target, parr, 1);
    }
    clazz = find_target_class(env, class_name);
    if (clazz == NULL) {
        return NULL;
    }
    target = resolve_method(env, clazz, method_name, signature);
    (*env)->DeleteLocalRef(env, clazz);
    if (target == NULL) {
        return NULL;
    }
    make_accessible(env, target);
    if (strcmp(method_name, "<init>") == 0) {
        (*env)->DeleteGlobalRef(env, target);
        return NULL;
    }
    ret = (*env)->CallObjectMethod(env, target, g_method_invoke,
                                   is_static ? NULL : (jobject)this_jobj, parr);
    if ((*env)->ExceptionCheck(env)) {
        log_jni_ex(env, "Method.invoke");
        ret = NULL;
    }
    (*env)->DeleteGlobalRef(env, target);
    return ret;
}

void* art_hook_new_instance(const char* class_name, const char* signature,
                            void* params) {
    JNIEnv* env;
    jclass clazz;
    jobject ctor;
    jobject obj;
    if (class_name == NULL) {
        return NULL;
    }
    if (art_hook_init() != 0) {
        return NULL;
    }
    env = t_env != NULL ? t_env : get_env();
    if (env == NULL) {
        return NULL;
    }
    clazz = find_target_class(env, class_name);
    if (clazz == NULL) {
        return NULL;
    }
    ctor = resolve_method(env, clazz, "<init>", signature);
    (*env)->DeleteLocalRef(env, clazz);
    if (ctor == NULL) {
        return NULL;
    }
    make_accessible(env, ctor);
    obj = (*env)->CallObjectMethod(env, ctor, g_ctor_new, (jobjectArray)params);
    if ((*env)->ExceptionCheck(env)) {
        log_jni_ex(env, "Constructor.newInstance");
        obj = NULL;
    }
    (*env)->DeleteGlobalRef(env, ctor);
    return obj;
}
