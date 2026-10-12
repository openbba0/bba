#ifndef BLACKBOX_GUM_ENGINE_H
#define BLACKBOX_GUM_ENGINE_H

#include <stddef.h>
#include <stdint.h>

// Frida Gum inline hook (Interceptor) and Stalker, arm64 only.
// The engine is the subset vendored from external_spindle.
class GumEngine {
public:
    using EnterLeave = void (*)(void *context, void *data);
    using EventFn = void (*)(int type, uintptr_t location, uintptr_t target, int depth, void *data);

    static void init();
    static void selfTest();

    // Interceptor.replace. Returns 0 on success and writes the original entry.
    static int replace(void *target, void *replacement, void **original);
    static void revert(void *target);

    // Interceptor.attach. Returns a listener handle, or nullptr.
    static void *attach(void *target, EnterLeave onEnter, EnterLeave onLeave, void *data);
    static void detach(void *listener);

    static uintptr_t arg(void *context, int index);
    static void setArg(void *context, int index, uintptr_t value);
    static uintptr_t retval(void *context);
    static void setRetval(void *context, uintptr_t value);

    // Stalker. threadId 0 follows the calling thread.
    // eventMask bits match GumEventType: call=1, ret=2, exec=4, block=8, compile=16.
    static bool stalkerFollow(uint64_t threadId, int eventMask, EventFn onEvent, void *data);
    static void stalkerUnfollow(uint64_t threadId);
    static void stalkerExclude(uintptr_t base, size_t size);
    static int stalkerEventCount();
};

#endif
