



#ifndef BLACKBOX_RUNTIMEHOOK_H
#define BLACKBOX_RUNTIMEHOOK_H


#include "base_hook.h"
#include <jni.h>

class RuntimeHook : public BaseHook {
public:
    static void init(JNIEnv *env);
};


#endif 
