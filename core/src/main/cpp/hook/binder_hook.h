



#ifndef BLACKBOX_BINDERHOOK_H
#define BLACKBOX_BINDERHOOK_H


#include "base_hook.h"

class BinderHook : public BaseHook{
public:
    static void init(JNIEnv *env);
};

#endif 
