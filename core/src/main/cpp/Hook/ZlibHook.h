#ifndef BLACKBOX_ZLIBHOOK_H
#define BLACKBOX_ZLIBHOOK_H

#include <jni.h>

class ZlibHook {
public:
    static void init(JNIEnv *env);
};

#endif
