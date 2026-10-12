#include "Hook/GumEngine.h"

#include "Log.h"

#include <atomic>

#if defined(__aarch64__)

#include <link.h>
#include <string.h>

#include "gum.h"

static GumInterceptor *g_interceptor = nullptr;
static GumStalker *g_stalker = nullptr;
static GumStalkerTransformer *g_transformer = nullptr;
static GumEventSink *g_sink = nullptr;
static GumThreadId g_followed_thread = 0;
static bool g_following = false;
static bool g_excluded_self = false;
static GumEngine::EventFn g_event_fn = nullptr;
static void *g_event_data = nullptr;
static std::atomic<int> g_event_count{0};

static int exclude_self(struct dl_phdr_info *info, size_t size, void *data) {
    (void) size;
    (void) data;
    if (info->dlpi_name == nullptr || strstr(info->dlpi_name, "libblackbox.so") == nullptr) {
        return 0;
    }
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];
        if (ph->p_type != PT_LOAD || (ph->p_flags & PF_X) == 0) {
            continue;
        }
        GumMemoryRange range;
        range.base_address = (GumAddress) (info->dlpi_addr + ph->p_vaddr);
        range.size = (gsize) ph->p_memsz;
        gum_stalker_exclude(g_stalker, &range);
    }
    return 0;
}

static void on_stalker_event(const GumEvent *event, GumCpuContext *cpu_context, gpointer user_data) {
    (void) cpu_context;
    (void) user_data;
    g_event_count.fetch_add(1, std::memory_order_relaxed);
    if (g_event_fn == nullptr || event == nullptr) {
        return;
    }
    uintptr_t location = 0;
    uintptr_t target = 0;
    int depth = 0;
    switch (event->type) {
        case GUM_CALL:
            location = (uintptr_t) event->call.location;
            target = (uintptr_t) event->call.target;
            depth = event->call.depth;
            break;
        case GUM_RET:
            location = (uintptr_t) event->ret.location;
            target = (uintptr_t) event->ret.target;
            depth = event->ret.depth;
            break;
        case GUM_EXEC:
            location = (uintptr_t) event->exec.location;
            break;
        case GUM_BLOCK:
            location = (uintptr_t) event->block.start;
            target = (uintptr_t) event->block.end;
            break;
        case GUM_COMPILE:
            location = (uintptr_t) event->compile.start;
            target = (uintptr_t) event->compile.end;
            break;
        default:
            break;
    }
    g_event_fn((int) event->type, location, target, depth, g_event_data);
}

static void ensure_stalker() {
    if (g_stalker != nullptr) {
        return;
    }
    g_stalker = gum_stalker_new();
    g_transformer = gum_stalker_transformer_make_default();
    if (!g_excluded_self && g_stalker != nullptr) {
        dl_iterate_phdr(exclude_self, nullptr);
        g_excluded_self = true;
    }
}

// Listener payload: [onEnter, onLeave, userData]. Freed on detach.
struct ListenerFns {
    GumEngine::EnterLeave onEnter;
    GumEngine::EnterLeave onLeave;
    void *data;
};

static void on_listener_enter(GumInvocationContext *context, gpointer data) {
    auto *fns = static_cast<ListenerFns *>(data);
    if (fns != nullptr && fns->onEnter != nullptr) {
        fns->onEnter(context, fns->data);
    }
}

static void on_listener_leave(GumInvocationContext *context, gpointer data) {
    auto *fns = static_cast<ListenerFns *>(data);
    if (fns != nullptr && fns->onLeave != nullptr) {
        fns->onLeave(context, fns->data);
    }
}

static void free_listener_fns(gpointer data) {
    delete static_cast<ListenerFns *>(data);
}

#endif

#if defined(__aarch64__)

__attribute__((noinline, optnone, used))
static int gum_probe(int x) {
    volatile int y = x;
    y = y + 1;
    return y;
}

static int g_hits = 0;
static void *g_probe_original = nullptr;
static int g_attach_arg = -1;

__attribute__((noinline, optnone))
static int gum_probe_hook(int x) {
    g_hits++;
    if (g_probe_original == nullptr) {
        return x;
    }
    return ((int (*)(int)) g_probe_original)(x) + 10;
}

static void on_probe_enter(void *context, void *data) {
    (void) data;
    g_attach_arg = (int) GumEngine::arg(context, 0);
}

__attribute__((noinline, optnone))
static int call_probe(int x) {
    return gum_probe(x);
}

static bool test_inline_hook() {
    g_hits = 0;
    g_attach_arg = -1;
    g_probe_original = nullptr;
    void *original = nullptr;
    int rc = gum_interceptor_replace(g_interceptor, (gpointer) gum_probe,
                                     (gpointer) gum_probe_hook, nullptr, &original);
    g_probe_original = original;
    int replaced = call_probe(1);
    gum_interceptor_revert(g_interceptor, (gpointer) gum_probe);
    bool replace_ok = rc == 0 && g_hits == 1 && replaced == 12;

    void *listener = GumEngine::attach((void *) gum_probe, on_probe_enter, nullptr, nullptr);
    int attached = call_probe(7);
    if (listener != nullptr) {
        GumEngine::detach(listener);
    }
    bool attach_ok = listener != nullptr && g_attach_arg == 7 && attached == 8;
    ALOGD("GumEngine inline replace=%s attach=%s rc=%d hits=%d value=%d arg=%d",
          replace_ok ? "ok" : "FAIL", attach_ok ? "ok" : "FAIL",
          rc, g_hits, replaced, g_attach_arg);
    return replace_ok && attach_ok;
}

static bool test_stalker() {
    if (!gum_stalker_is_supported()) {
        ALOGD("GumEngine stalker unsupported");
        return false;
    }
    if (g_stalker == nullptr) {
        g_stalker = gum_stalker_new();
        g_transformer = gum_stalker_transformer_make_default();
    }
    if (g_stalker == nullptr || g_transformer == nullptr) {
        return false;
    }
    g_event_fn = nullptr;
    g_event_count.store(0, std::memory_order_relaxed);
    if (g_sink != nullptr) {
        g_object_unref(g_sink);
    }
    g_sink = gum_event_sink_make_from_callback((GumEventType) (GUM_CALL | GUM_COMPILE),
                                               on_stalker_event, nullptr, nullptr);
    gum_stalker_follow_me(g_stalker, g_transformer, g_sink);
    int n = call_probe(3);
    n += (int) strlen("gum");
    gum_stalker_unfollow_me(g_stalker);
    gum_stalker_flush(g_stalker);
    g_following = false;
    int events = g_event_count.load(std::memory_order_relaxed);
    bool ok = events > 0 && n == 7;
    ALOGD("GumEngine stalker %s events=%d", ok ? "ok" : "FAIL", events);
    return ok;
}

#endif

void GumEngine::init() {
#if defined(__aarch64__)
    static bool ready = false;
    gum_init();
    if (g_interceptor == nullptr) {
        g_interceptor = gum_interceptor_obtain();
    }
    if (!ready) {
        ready = true;
        ALOGD("GumEngine ready");
    }
#else
    ALOGD("GumEngine is arm64 only");
#endif
}

void GumEngine::selfTest() {
#if defined(__aarch64__)
    static bool tested = false;
    if (tested) {
        return;
    }
    tested = true;
    init();
    bool inline_ok = g_interceptor != nullptr && test_inline_hook();
    bool stalker_ok = test_stalker();
    ALOGD("GumEngine self-test inline=%s stalker=%s",
          inline_ok ? "ok" : "FAIL", stalker_ok ? "ok" : "FAIL");
#else
    ALOGD("GumEngine self-test skipped");
#endif
}

int GumEngine::replace(void *target, void *replacement, void **original) {
#if defined(__aarch64__)
    init();
    if (g_interceptor == nullptr || target == nullptr || replacement == nullptr) {
        return -1;
    }
    return (int) gum_interceptor_replace(g_interceptor, target, replacement, nullptr, original);
#else
    (void) target;
    (void) replacement;
    (void) original;
    return -1;
#endif
}

void GumEngine::revert(void *target) {
#if defined(__aarch64__)
    init();
    if (g_interceptor != nullptr && target != nullptr) {
        gum_interceptor_revert(g_interceptor, target);
    }
#else
    (void) target;
#endif
}

void *GumEngine::attach(void *target, EnterLeave onEnter, EnterLeave onLeave, void *data) {
#if defined(__aarch64__)
    init();
    if (g_interceptor == nullptr || target == nullptr) {
        return nullptr;
    }
    auto *fns = new ListenerFns{onEnter, onLeave, data};
    GumInvocationListener *listener = gum_make_call_listener(
            on_listener_enter, on_listener_leave, fns, free_listener_fns);
    if (gum_interceptor_attach(g_interceptor, target, listener, nullptr) != GUM_ATTACH_OK) {
        g_object_unref(listener);
        return nullptr;
    }
    return listener;
#else
    (void) target;
    (void) onEnter;
    (void) onLeave;
    (void) data;
    return nullptr;
#endif
}

void GumEngine::detach(void *listener) {
#if defined(__aarch64__)
    init();
    if (g_interceptor == nullptr || listener == nullptr) {
        return;
    }
    auto *typed = static_cast<GumInvocationListener *>(listener);
    gum_interceptor_detach(g_interceptor, typed);
    g_object_unref(typed);
#else
    (void) listener;
#endif
}

uintptr_t GumEngine::arg(void *context, int index) {
#if defined(__aarch64__)
    if (context == nullptr || index < 0) {
        return 0;
    }
    return (uintptr_t) gum_invocation_context_get_nth_argument(
            static_cast<GumInvocationContext *>(context), (guint) index);
#else
    (void) context;
    (void) index;
    return 0;
#endif
}

void GumEngine::setArg(void *context, int index, uintptr_t value) {
#if defined(__aarch64__)
    if (context == nullptr || index < 0) {
        return;
    }
    gum_invocation_context_replace_nth_argument(
            static_cast<GumInvocationContext *>(context), (guint) index, (gpointer) value);
#else
    (void) context;
    (void) index;
    (void) value;
#endif
}

uintptr_t GumEngine::retval(void *context) {
#if defined(__aarch64__)
    if (context == nullptr) {
        return 0;
    }
    return (uintptr_t) gum_invocation_context_get_return_value(
            static_cast<GumInvocationContext *>(context));
#else
    (void) context;
    return 0;
#endif
}

void GumEngine::setRetval(void *context, uintptr_t value) {
#if defined(__aarch64__)
    if (context == nullptr) {
        return;
    }
    gum_invocation_context_replace_return_value(
            static_cast<GumInvocationContext *>(context), (gpointer) value);
#else
    (void) context;
    (void) value;
#endif
}

bool GumEngine::stalkerFollow(uint64_t threadId, int eventMask, EventFn onEvent, void *data) {
#if defined(__aarch64__)
    init();
    if (!gum_stalker_is_supported()) {
        return false;
    }
    ensure_stalker();
    if (g_stalker == nullptr || g_transformer == nullptr) {
        return false;
    }
    if (g_following) {
        if (g_followed_thread == 0 || g_followed_thread == gum_process_get_current_thread_id()) {
            gum_stalker_unfollow_me(g_stalker);
        } else {
            gum_stalker_unfollow(g_stalker, g_followed_thread);
        }
        g_following = false;
    }
    g_event_fn = onEvent;
    g_event_data = data;
    g_event_count.store(0, std::memory_order_relaxed);
    if (g_sink != nullptr) {
        g_object_unref(g_sink);
        g_sink = nullptr;
    }
    g_sink = gum_event_sink_make_from_callback((GumEventType) eventMask, on_stalker_event, nullptr, nullptr);
    if (g_sink == nullptr) {
        return false;
    }
    GumThreadId tid = (GumThreadId) threadId;
    if (tid == 0 || tid == gum_process_get_current_thread_id()) {
        gum_stalker_follow_me(g_stalker, g_transformer, g_sink);
        g_followed_thread = gum_process_get_current_thread_id();
    } else {
        gum_stalker_follow(g_stalker, tid, g_transformer, g_sink);
        g_followed_thread = tid;
    }
    g_following = true;
    return true;
#else
    (void) threadId;
    (void) eventMask;
    (void) onEvent;
    (void) data;
    return false;
#endif
}

void GumEngine::stalkerUnfollow(uint64_t threadId) {
#if defined(__aarch64__)
    if (g_stalker == nullptr || !g_following) {
        return;
    }
    GumThreadId tid = (GumThreadId) threadId;
    if (tid == 0 || tid == g_followed_thread ||
        g_followed_thread == gum_process_get_current_thread_id()) {
        if (g_followed_thread == gum_process_get_current_thread_id()) {
            gum_stalker_unfollow_me(g_stalker);
        } else {
            gum_stalker_unfollow(g_stalker, g_followed_thread);
        }
    } else {
        gum_stalker_unfollow(g_stalker, tid);
    }
    gum_stalker_flush(g_stalker);
    g_following = false;
#else
    (void) threadId;
#endif
}

void GumEngine::stalkerExclude(uintptr_t base, size_t size) {
#if defined(__aarch64__)
    init();
    ensure_stalker();
    if (g_stalker == nullptr || base == 0 || size == 0) {
        return;
    }
    GumMemoryRange range;
    range.base_address = (GumAddress) base;
    range.size = (gsize) size;
    gum_stalker_exclude(g_stalker, &range);
#else
    (void) base;
    (void) size;
#endif
}

int GumEngine::stalkerEventCount() {
#if defined(__aarch64__)
    return g_event_count.load(std::memory_order_relaxed);
#else
    return 0;
#endif
}
