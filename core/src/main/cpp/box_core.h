



#ifndef VIRTUALM_VMCORE_H
#define VIRTUALM_VMCORE_H

#include <jni.h>
#include <string>

#define VMCORE_CLASS "com/android/bb/core/NativeCore"

class BoxCore {
public:
    static JavaVM *getJavaVM();
    static int getApiLevel();
    static int getCallingUid(JNIEnv *env, int orig);
    static jstring redirectPathString(JNIEnv *env, jstring path);
    static jobject redirectPathFile(JNIEnv *env, jobject path);
    static void bindSpindle(JNIEnv *env);
    static void onSpindleScript(const void *data, int len);
    static bool deliverSpindle(int id, const void *data, int len, std::string *reply);
    static bool launchForSpindle(const char *packageName);
    static bool launchForSpindleOnMain(const char *packageName);
};


#endif 
