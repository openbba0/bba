



#ifndef BLACKBOX_VMCLASSLOADERHOOK_H
#define BLACKBOX_VMCLASSLOADERHOOK_H


#include "base_hook.h"
#include <jni.h>

class VMClassLoaderHook : public BaseHook {
public:
    static void hideXposed();
    static void init(JNIEnv *env);
};


#endif 
