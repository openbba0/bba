



#ifndef VIRTUALM_UNIXFILESYSTEMHOOK_H
#define VIRTUALM_UNIXFILESYSTEMHOOK_H


#include "base_hook.h"

class UnixFileSystemHook : public BaseHook {
public:
    static void init(JNIEnv *env);
};


#endif 
