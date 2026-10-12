#include "dex_file_hook.h"
#include <io.h>
#include <box_core.h>
#include "unix_file_system_hook.h"
#import "jnihook/jni_hook.h"
#include <sys/stat.h>

HOOK_JNI(jobject, openDexFileNative, JNIEnv *env, jobject obj,jstring sourceName, jstring outputName, jint flags,jobject loader, jobject elements) {
    const char *sourceNameC = env->GetStringUTFChars(sourceName, JNI_FALSE);
    ALOGD("openDexFileNative: %s", sourceNameC);
    if(strstr(sourceNameC,"/blackbox/") != nullptr){




        DexFileHook::setFileReadonly(sourceNameC);
    }
    jobject orig = orig_openDexFileNative(env, obj,sourceName,outputName,flags,loader,elements);
    env->ReleaseStringUTFChars(sourceName, sourceNameC);
    return orig;
}


void DexFileHook::init(JNIEnv *env) {
    if (BoxCore::getApiLevel() >= __ANDROID_API_U__) {
        const char *clazz = "dalvik/system/DexFile";
        JniHook::HookJniFun(env, clazz, "openDexFileNative", "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/ClassLoader;[Ldalvik/system/DexPathList$Element;)Ljava/lang/Object;", (void *) new_openDexFileNative,
                            (void **) (&orig_openDexFileNative), true);
    }
}

void DexFileHook::setFileReadonly(const char* filePath) {
    struct stat fileStat;

    
    if (stat(filePath, &fileStat) != 0) {
        ALOGD("DexFileHook::setFileReadonly: %s 不存在",filePath);
        return;
    }

    
    
    if (chmod(filePath, S_IRUSR) != 0) {
        ALOGD("DexFileHook::setFileReadonly: 设置文件 %s 为只读时出错",filePath);
    } else {
        ALOGD("DexFileHook::setFileReadonly: 设置文件 %s 为只读成功",filePath);
    }
}
