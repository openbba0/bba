#include "stalker_js.h"
#include "spindle_str.h"
#include "sp_throw.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gum.h"
#include "gumstalker.h"

#include <capstone/capstone.h>
#include <elf.h>
#include <link.h>
#include <pthread.h>

/* Plaintext XOR 0xA7, including the terminating NUL. Decoded on the stack. */
/* "stalker init failed" */
static const unsigned char enc_stalker_init[] = {
    0xd4, 0xd3, 0xc6, 0xcb, 0xcc, 0xc2, 0xd5, 0x87, 0xce, 0xc9, 0xce, 0xd3,
    0x87, 0xc1, 0xc6, 0xce, 0xcb, 0xc2, 0xc3, 0xa7
};
/* "Stalker" */
static const unsigned char enc_stalker[] = {
    0xf4, 0xd3, 0xc6, 0xcb, 0xcc, 0xc2, 0xd5, 0xa7
};
/* "Stalker.follow(threadId, options)" */
static const unsigned char enc_stalker_follow[] = {
    0xf4, 0xd3, 0xc6, 0xcb, 0xcc, 0xc2, 0xd5, 0x89, 0xc1, 0xc8, 0xcb, 0xcb, 0xc8, 0xd0,
    0x8f, 0xd3, 0xcf, 0xd5, 0xc2, 0xc6, 0xc3, 0xee, 0xc3, 0x8b, 0x87, 0xc8, 0xd7, 0xd3,
    0xce, 0xc8, 0xc9, 0xd4, 0x8e, 0xa7
};
/* "Stalker.unfollow(threadId)" */
static const unsigned char enc_stalker_unfollow[] = {
    0xf4, 0xd3, 0xc6, 0xcb, 0xcc, 0xc2, 0xd5, 0x89, 0xd2, 0xc9, 0xc1, 0xc8, 0xcb, 0xcb,
    0xc8, 0xd0, 0x8f, 0xd3, 0xcf, 0xd5, 0xc2, 0xc6, 0xc3, 0xee, 0xc3, 0x8e, 0xa7
};
/* "Stalker.exclude(base, size)" */
static const unsigned char enc_stalker_exclude[] = {
    0xf4, 0xd3, 0xc6, 0xcb, 0xcc, 0xc2, 0xd5, 0x89, 0xc2, 0xdf, 0xc4, 0xcb, 0xd2, 0xc3,
    0xc2, 0x8f, 0xc5, 0xc6, 0xd4, 0xc2, 0x8b, 0x87, 0xd4, 0xce, 0xdd, 0xc2, 0x8e, 0xa7
};
/* "Stalker.disasm(address)" */
static const unsigned char enc_stalker_disasm[] = {
    0xf4, 0xd3, 0xc6, 0xcb, 0xcc, 0xc2, 0xd5, 0x89, 0xc3, 0xce, 0xd4, 0xc6, 0xd4, 0xca,
    0x8f, 0xc6, 0xc3, 0xc3, 0xd5, 0xc2, 0xd4, 0xd4, 0x8e, 0xa7
};
/* "onReceive" */
static const unsigned char enc_on_receive[] = {
    0xc8, 0xc9, 0xf5, 0xc2, 0xc4, 0xc2, 0xce, 0xd1, 0xc2, 0xa7
};
/* "onCallSummary" */
static const unsigned char enc_on_call_summary[] = {
    0xc8, 0xc9, 0xe4, 0xc6, 0xcb, 0xcb, 0xf4, 0xd2, 0xca, 0xca, 0xc6, 0xd5, 0xde, 0xa7
};
/* "Stalker.parse(events)" */
static const unsigned char enc_stalker_parse[] = {
    0xf4, 0xd3, 0xc6, 0xcb, 0xcc, 0xc2, 0xd5, 0x89, 0xd7, 0xc6, 0xd5, 0xd4, 0xc2, 0x8f,
    0xc2, 0xd1, 0xc2, 0xc9, 0xd3, 0xd4, 0x8e, 0xa7
};
/* "follow" */
static const unsigned char enc_follow[] = {
    0xc1, 0xc8, 0xcb, 0xcb, 0xc8, 0xd0, 0xa7
};
/* "unfollow" */
static const unsigned char enc_unfollow[] = {
    0xd2, 0xc9, 0xc1, 0xc8, 0xcb, 0xcb, 0xc8, 0xd0, 0xa7
};
/* "flush" */
static const unsigned char enc_flush[] = {
    0xc1, 0xcb, 0xd2, 0xd4, 0xcf, 0xa7
};
/* "exclude" */
static const unsigned char enc_exclude[] = {
    0xc2, 0xdf, 0xc4, 0xcb, 0xd2, 0xc3, 0xc2, 0xa7
};
/* "parse" */
static const unsigned char enc_parse[] = {
    0xd7, 0xc6, 0xd5, 0xd4, 0xc2, 0xa7
};
/* "disasm" */
static const unsigned char enc_disasm[] = {
    0xc3, 0xce, 0xd4, 0xc6, 0xd4, 0xca, 0xa7
};
/* "transform" */
static const unsigned char enc_transform[] = {
    0xd3, 0xd5, 0xc6, 0xc9, 0xd4, 0xc1, 0xc8, 0xd5, 0xca, 0xa7
};
/* "events" */
static const unsigned char enc_events[] = {
    0xc2, 0xd1, 0xc2, 0xc9, 0xd3, 0xd4, 0xa7
};
/* "call" */
static const unsigned char enc_call[] = {
    0xc4, 0xc6, 0xcb, 0xcb, 0xa7
};
/* "ret" */
static const unsigned char enc_ret[] = {
    0xd5, 0xc2, 0xd3, 0xa7
};
/* "exec" */
static const unsigned char enc_exec[] = {
    0xc2, 0xdf, 0xc2, 0xc4, 0xa7
};
/* "block" */
static const unsigned char enc_block[] = {
    0xc5, 0xcb, 0xc8, 0xc4, 0xcc, 0xa7
};
/* "compile" */
static const unsigned char enc_compile[] = {
    0xc4, 0xc8, 0xca, 0xd7, 0xce, 0xcb, 0xc2, 0xa7
};
/* "nothing" */
static const unsigned char enc_nothing[] = {
    0xc9, 0xc8, 0xd3, 0xcf, 0xce, 0xc9, 0xc0, 0xa7
};
/* "type" */
static const unsigned char enc_type[] = {
    0xd3, 0xde, 0xd7, 0xc2, 0xa7
};
/* "location" */
static const unsigned char enc_location[] = {
    0xcb, 0xc8, 0xc4, 0xc6, 0xd3, 0xce, 0xc8, 0xc9, 0xa7
};
/* "target" */
static const unsigned char enc_target[] = {
    0xd3, 0xc6, 0xd5, 0xc0, 0xc2, 0xd3, 0xa7
};
/* "depth" */
static const unsigned char enc_depth[] = {
    0xc3, 0xc2, 0xd7, 0xd3, 0xcf, 0xa7
};
/* "start" */
static const unsigned char enc_start[] = {
    0xd4, 0xd3, 0xc6, 0xd5, 0xd3, 0xa7
};
/* "end" */
static const unsigned char enc_end[] = {
    0xc2, 0xc9, 0xc3, 0xa7
};
/* "transform is not supported" */
static const unsigned char enc_transform_unsupported[] = {
    0xd3, 0xd5, 0xc6, 0xc9, 0xd4, 0xc1, 0xc8, 0xd5, 0xca, 0x87, 0xce, 0xd4, 0x87,
    0xc9, 0xc8, 0xd3, 0x87, 0xd4, 0xd2, 0xd7, 0xd7, 0xc8, 0xd5, 0xd3, 0xc2, 0xc3,
    0xa7
};

#define THROW_ENC(ctx, enc) SP_THROW(ctx, enc)

static GumStalker* g_stalker = NULL;
static GumStalkerTransformer* g_transformer = NULL;
static GumEventSink* g_sink = NULL;
static JSValue g_on_receive;
static JSValue g_on_summary;
static int g_have_callbacks = 0;
static int g_excluded_runtime = 0;
static GumThreadId g_tid = 0;
static int g_following = 0;

static pthread_mutex_t g_ev_mu = PTHREAD_MUTEX_INITIALIZER;
static uint8_t* g_events = NULL;
static size_t g_events_len = 0;
static size_t g_events_cap = 0;

struct call_count {
    uintptr_t target;
    uint32_t count;
};

static struct call_count* g_counts = NULL;
static size_t g_count_n = 0;
static size_t g_count_cap = 0;

static int js_flag(JSContext* ctx, JSValueConst obj, const char* name) {
    JSValue v;
    int r;
    if (!JS_IsObject(obj)) {
        return 0;
    }
    v = JS_GetPropertyStr(ctx, obj, name);
    r = JS_ToBool(ctx, v) == 1;
    JS_FreeValue(ctx, v);
    return r;
}

static int to_u64(JSContext* ctx, JSValueConst val, uint64_t* out) {
    return spindle_js_to_u64(ctx, val, out);
}

static void note_call(uintptr_t target) {
    size_t i;
    for (i = 0; i < g_count_n; i++) {
        if (g_counts[i].target == target) {
            g_counts[i].count++;
            return;
        }
    }
    if (g_count_n == g_count_cap) {
        size_t cap = g_count_cap == 0 ? 64 : g_count_cap * 2;
        struct call_count* next = realloc(g_counts, cap * sizeof(*next));
        if (next == NULL) {
            return;
        }
        g_counts = next;
        g_count_cap = cap;
    }
    g_counts[g_count_n].target = target;
    g_counts[g_count_n].count = 1;
    g_count_n++;
}

static void on_event(const GumEvent* event, GumCpuContext* cpu_context, gpointer user_data) {
    (void)cpu_context;
    (void)user_data;
    pthread_mutex_lock(&g_ev_mu);
    if (g_events_len + sizeof(*event) > g_events_cap) {
        size_t cap = g_events_cap == 0 ? 4096 : g_events_cap * 2;
        uint8_t* next;
        while (cap < g_events_len + sizeof(*event)) {
            cap *= 2;
        }
        next = realloc(g_events, cap);
        if (next == NULL) {
            pthread_mutex_unlock(&g_ev_mu);
            return;
        }
        g_events = next;
        g_events_cap = cap;
    }
    memcpy(g_events + g_events_len, event, sizeof(*event));
    g_events_len += sizeof(*event);
    if (event->type == GUM_CALL) {
        note_call((uintptr_t)event->call.target);
    }
    pthread_mutex_unlock(&g_ev_mu);
}

static int exclude_runtime(struct dl_phdr_info* info, size_t size, void* data) {
    int i;
    (void)size;
    (void)data;
    if (info->dlpi_name == NULL || strstr(info->dlpi_name, "libandroid_runtime.so") == NULL) {
        return 0;
    }
    for (i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)* ph = &info->dlpi_phdr[i];
        GumMemoryRange range;
        if (ph->p_type != PT_LOAD || (ph->p_flags & PF_X) == 0) {
            continue;
        }
        range.base_address = (GumAddress)(info->dlpi_addr + ph->p_vaddr);
        range.size = (gsize)ph->p_memsz;
        gum_stalker_exclude(g_stalker, &range);
    }
    return 0;
}

static void ensure_stalker(void) {
    if (g_stalker != NULL) {
        return;
    }
    gum_init();
    g_stalker = gum_stalker_new();
    g_transformer = gum_stalker_transformer_make_default();
    g_sink = gum_event_sink_make_from_callback(GUM_NOTHING, on_event, NULL, NULL);
    if (!g_excluded_runtime) {
        dl_iterate_phdr(exclude_runtime, NULL);
        g_excluded_runtime = 1;
    }
}

static void clear_queue(void) {
    pthread_mutex_lock(&g_ev_mu);
    g_events_len = 0;
    g_count_n = 0;
    pthread_mutex_unlock(&g_ev_mu);
}

static void deliver(JSContext* ctx) {
    uint8_t* bytes = NULL;
    size_t len = 0;
    struct call_count* counts = NULL;
    size_t count_n = 0;
    size_t i;

    gum_stalker_flush(g_stalker);
    pthread_mutex_lock(&g_ev_mu);
    len = g_events_len;
    if (len > 0) {
        bytes = malloc(len);
        if (bytes != NULL) {
            memcpy(bytes, g_events, len);
        }
    }
    count_n = g_count_n;
    if (count_n > 0) {
        counts = malloc(count_n * sizeof(*counts));
        if (counts != NULL) {
            memcpy(counts, g_counts, count_n * sizeof(*counts));
        } else {
            count_n = 0;
        }
    }
    g_events_len = 0;
    g_count_n = 0;
    pthread_mutex_unlock(&g_ev_mu);

    if (g_have_callbacks && JS_IsFunction(ctx, g_on_receive) && bytes != NULL) {
        JSValue buf = JS_NewArrayBufferCopy(ctx, bytes, len);
        JSValue ret = JS_Call(ctx, g_on_receive, JS_UNDEFINED, 1, &buf);
        if (JS_IsException(ret)) {
            JSValue ex = JS_GetException(ctx);
            JS_FreeValue(ctx, ex);
        }
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, buf);
    }
    if (g_have_callbacks && JS_IsFunction(ctx, g_on_summary)) {
        JSValue summary = JS_NewObject(ctx);
        for (i = 0; i < count_n; i++) {
            char key[32];
            snprintf(key, sizeof(key), "0x%llx", (unsigned long long)counts[i].target);
            JS_SetPropertyStr(ctx, summary, key, JS_NewInt32(ctx, (int32_t)counts[i].count));
        }
        JSValue ret = JS_Call(ctx, g_on_summary, JS_UNDEFINED, 1, &summary);
        if (JS_IsException(ret)) {
            JSValue ex = JS_GetException(ctx);
            JS_FreeValue(ctx, ex);
        }
        JS_FreeValue(ctx, ret);
        JS_FreeValue(ctx, summary);
    }
    free(bytes);
    free(counts);
}

static JSValue js_stalker_follow(JSContext* ctx, JSValueConst this_val, int argc,
                                 JSValueConst* argv) {
    uint64_t thread_id = 0;
    JSValue options;
    JSValue transform;
    JSValue events;
    JSValue on_receive;
    JSValue on_summary;
    GumEventType mask = GUM_NOTHING;
    GumThreadId tid;
    int options_index;
    (void)this_val;
    if (argc >= 2 && JS_IsObject(argv[1])) {
        if (to_u64(ctx, argv[0], &thread_id) != 0) {
            THROW_ENC(ctx, enc_stalker_follow);
        }
        options_index = 1;
    } else if (argc == 1 && JS_IsObject(argv[0]) && to_u64(ctx, argv[0], &thread_id) != 0) {
        JSValue ex = JS_GetException(ctx);
        JS_FreeValue(ctx, ex);
        thread_id = 0;
        options_index = 0;
    } else if (argc == 1) {
        if (to_u64(ctx, argv[0], &thread_id) != 0) {
            THROW_ENC(ctx, enc_stalker_follow);
        }
        options_index = -1;
    } else if (argc == 0) {
        options_index = -1;
    } else {
        THROW_ENC(ctx, enc_stalker_follow);
    }
    options = options_index < 0 ? JS_UNDEFINED : argv[options_index];
    {
        char k_transform[sizeof(enc_transform)];
        char k_events[sizeof(enc_events)];
        char k_call[sizeof(enc_call)];
        char k_ret[sizeof(enc_ret)];
        char k_exec[sizeof(enc_exec)];
        char k_block[sizeof(enc_block)];
        char k_compile[sizeof(enc_compile)];
        sp_dec(k_transform, enc_transform, sizeof(enc_transform));
        sp_dec(k_events, enc_events, sizeof(enc_events));
        sp_dec(k_call, enc_call, sizeof(enc_call));
        sp_dec(k_ret, enc_ret, sizeof(enc_ret));
        sp_dec(k_exec, enc_exec, sizeof(enc_exec));
        sp_dec(k_block, enc_block, sizeof(enc_block));
        sp_dec(k_compile, enc_compile, sizeof(enc_compile));
        transform = JS_IsUndefined(options) ? JS_UNDEFINED
                                            : JS_GetPropertyStr(ctx, options, k_transform);
        if (!JS_IsUndefined(transform) && !JS_IsNull(transform)) {
            JS_FreeValue(ctx, transform);
            SP_THROW(ctx, enc_transform_unsupported);
        }
        JS_FreeValue(ctx, transform);
        events = JS_IsUndefined(options) ? JS_UNDEFINED : JS_GetPropertyStr(ctx, options, k_events);
        if (js_flag(ctx, events, k_call)) {
            mask |= GUM_CALL;
        }
        if (js_flag(ctx, events, k_ret)) {
            mask |= GUM_RET;
        }
        if (js_flag(ctx, events, k_exec)) {
            mask |= GUM_EXEC;
        }
        if (js_flag(ctx, events, k_block)) {
            mask |= GUM_BLOCK;
        }
        if (js_flag(ctx, events, k_compile)) {
            mask |= GUM_COMPILE;
        }
        JS_FreeValue(ctx, events);
    }
    {
        char receive_name[sizeof(enc_on_receive)];
        char summary_name[sizeof(enc_on_call_summary)];
        sp_dec(receive_name, enc_on_receive, sizeof(enc_on_receive));
        sp_dec(summary_name, enc_on_call_summary, sizeof(enc_on_call_summary));
        on_receive = JS_IsUndefined(options) ? JS_UNDEFINED
                                             : JS_GetPropertyStr(ctx, options, receive_name);
        on_summary = JS_IsUndefined(options) ? JS_UNDEFINED
                                             : JS_GetPropertyStr(ctx, options, summary_name);
    }
    if (!JS_IsFunction(ctx, on_receive)) {
        JS_FreeValue(ctx, on_receive);
        on_receive = JS_UNDEFINED;
    }
    if (!JS_IsFunction(ctx, on_summary)) {
        JS_FreeValue(ctx, on_summary);
        on_summary = JS_UNDEFINED;
    }

    ensure_stalker();
    if (g_stalker == NULL || g_transformer == NULL || g_sink == NULL) {
        JS_FreeValue(ctx, on_receive);
        JS_FreeValue(ctx, on_summary);
        SP_THROW_INTERNAL(ctx, enc_stalker_init);
    }
    if (g_following) {
        if (g_tid == 0 || g_tid == gum_process_get_current_thread_id()) {
            gum_stalker_unfollow_me(g_stalker);
        } else {
            gum_stalker_unfollow(g_stalker, g_tid);
        }
        g_following = 0;
    }
    clear_queue();
    if (g_have_callbacks) {
        JS_FreeValue(ctx, g_on_receive);
        JS_FreeValue(ctx, g_on_summary);
    }
    g_on_receive = on_receive;
    g_on_summary = on_summary;
    g_have_callbacks = 1;
    g_object_unref(g_sink);
    g_sink = gum_event_sink_make_from_callback(mask, on_event, NULL, NULL);

    tid = (GumThreadId)thread_id;
    if (tid == 0 || tid == gum_process_get_current_thread_id()) {
        gum_stalker_follow_me(g_stalker, g_transformer, g_sink);
        g_tid = gum_process_get_current_thread_id();
    } else {
        gum_stalker_follow(g_stalker, tid, g_transformer, g_sink);
        g_tid = tid;
    }
    g_following = 1;
    return JS_UNDEFINED;
}

static JSValue js_stalker_unfollow(JSContext* ctx, JSValueConst this_val, int argc,
                                   JSValueConst* argv) {
    uint64_t thread_id = 0;
    (void)this_val;
    if (g_stalker == NULL || !g_following) {
        return JS_UNDEFINED;
    }
    if (argc >= 1 && !JS_IsUndefined(argv[0])) {
        if (to_u64(ctx, argv[0], &thread_id) != 0) {
            THROW_ENC(ctx, enc_stalker_unfollow);
        }
    }
    if (thread_id == 0 || (GumThreadId)thread_id == g_tid ||
        g_tid == gum_process_get_current_thread_id()) {
        if (g_tid == gum_process_get_current_thread_id()) {
            gum_stalker_unfollow_me(g_stalker);
        } else {
            gum_stalker_unfollow(g_stalker, g_tid);
        }
    } else {
        gum_stalker_unfollow(g_stalker, (GumThreadId)thread_id);
    }
    g_following = 0;
    return JS_UNDEFINED;
}

static JSValue js_stalker_flush(JSContext* ctx, JSValueConst this_val, int argc,
                                JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    if (g_stalker == NULL) {
        return JS_UNDEFINED;
    }
    deliver(ctx);
    return JS_UNDEFINED;
}

static JSValue js_stalker_exclude(JSContext* ctx, JSValueConst this_val, int argc,
                                  JSValueConst* argv) {
    uint64_t base = 0;
    uint64_t size = 0;
    GumMemoryRange range;
    (void)this_val;
    if (argc == 1 && JS_IsObject(argv[0])) {
        JSValue b = JS_GetPropertyStr(ctx, argv[0], "base");
        JSValue s = JS_GetPropertyStr(ctx, argv[0], "size");
        int bad = to_u64(ctx, b, &base) != 0 || to_u64(ctx, s, &size) != 0;
        JS_FreeValue(ctx, b);
        JS_FreeValue(ctx, s);
        if (bad) {
            THROW_ENC(ctx, enc_stalker_exclude);
        }
    } else if (argc < 2 || to_u64(ctx, argv[0], &base) != 0 || to_u64(ctx, argv[1], &size) != 0) {
        THROW_ENC(ctx, enc_stalker_exclude);
    }
    ensure_stalker();
    if (g_stalker == NULL) {
        SP_THROW_INTERNAL(ctx, enc_stalker_init);
    }
    range.base_address = (GumAddress)base;
    range.size = (gsize)size;
    gum_stalker_exclude(g_stalker, &range);
    return JS_UNDEFINED;
}

static void event_name(GumEventType type, char* dst) {
    if (type == GUM_CALL) {
        sp_dec(dst, enc_call, sizeof(enc_call));
    } else if (type == GUM_RET) {
        sp_dec(dst, enc_ret, sizeof(enc_ret));
    } else if (type == GUM_EXEC) {
        sp_dec(dst, enc_exec, sizeof(enc_exec));
    } else if (type == GUM_BLOCK) {
        sp_dec(dst, enc_block, sizeof(enc_block));
    } else if (type == GUM_COMPILE) {
        sp_dec(dst, enc_compile, sizeof(enc_compile));
    } else {
        sp_dec(dst, enc_nothing, sizeof(enc_nothing));
    }
}

static csh g_cs = 0;
static int g_cs_ready = 0;

static int ensure_cs(void) {
    if (g_cs_ready) {
        return g_cs != 0;
    }
    g_cs_ready = 1;
    if (cs_open(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, &g_cs) != CS_ERR_OK) {
        g_cs = 0;
        return 0;
    }
    cs_option(g_cs, CS_OPT_DETAIL, CS_OPT_OFF);
    return 1;
}

static JSValue js_stalker_disasm(JSContext* ctx, JSValueConst this_val, int argc,
                                 JSValueConst* argv) {
    uint64_t addr = 0;
    cs_insn* insn = NULL;
    size_t n;
    char text[160];
    uint32_t word = 0;
    (void)this_val;
    if (argc < 1 || to_u64(ctx, argv[0], &addr) != 0 || addr == 0) {
        THROW_ENC(ctx, enc_stalker_disasm);
    }
    memcpy(&word, (const void*)(uintptr_t)addr, sizeof(word));
    if (!ensure_cs()) {
        snprintf(text, sizeof(text), ".word 0x%08x", word);
        return JS_NewString(ctx, text);
    }
    n = cs_disasm(g_cs, (const uint8_t*)(uintptr_t)addr, 4, addr, 1, &insn);
    if (n == 0 || insn == NULL) {
        snprintf(text, sizeof(text), ".word 0x%08x", word);
        return JS_NewString(ctx, text);
    }
    if (insn->op_str[0] != '\0') {
        snprintf(text, sizeof(text), "%s %s", insn->mnemonic, insn->op_str);
    } else {
        snprintf(text, sizeof(text), "%s", insn->mnemonic);
    }
    cs_free(insn, n);
    return JS_NewString(ctx, text);
}

static JSValue js_stalker_parse(JSContext* ctx, JSValueConst this_val, int argc,
                                JSValueConst* argv) {
    size_t n = 0;
    uint8_t* bytes;
    size_t count;
    size_t i;
    JSValue arr;
    (void)this_val;
    if (argc < 1) {
        THROW_ENC(ctx, enc_stalker_parse);
    }
    bytes = JS_GetArrayBuffer(ctx, &n, argv[0]);
    if (bytes == NULL || (n % sizeof(GumEvent)) != 0) {
        THROW_ENC(ctx, enc_stalker_parse);
    }
    count = n / sizeof(GumEvent);
    arr = JS_NewArray(ctx);
    {
        char k_type[sizeof(enc_type)];
        char k_location[sizeof(enc_location)];
        char k_target[sizeof(enc_target)];
        char k_depth[sizeof(enc_depth)];
        char k_start[sizeof(enc_start)];
        char k_end[sizeof(enc_end)];
        sp_dec(k_type, enc_type, sizeof(enc_type));
        sp_dec(k_location, enc_location, sizeof(enc_location));
        sp_dec(k_target, enc_target, sizeof(enc_target));
        sp_dec(k_depth, enc_depth, sizeof(enc_depth));
        sp_dec(k_start, enc_start, sizeof(enc_start));
        sp_dec(k_end, enc_end, sizeof(enc_end));
        for (i = 0; i < count; i++) {
            const GumEvent* event = (const GumEvent*)(bytes + i * sizeof(GumEvent));
            JSValue obj = JS_NewObject(ctx);
            char type_name[16];
            event_name(event->type, type_name);
            JS_SetPropertyStr(ctx, obj, k_type, JS_NewString(ctx, type_name));
            if (event->type == GUM_CALL || event->type == GUM_RET) {
                JS_SetPropertyStr(ctx, obj, k_location,
                                  JS_NewInt64(ctx, (int64_t)(uintptr_t)event->call.location));
                JS_SetPropertyStr(ctx, obj, k_target,
                                  JS_NewInt64(ctx, (int64_t)(uintptr_t)event->call.target));
                JS_SetPropertyStr(ctx, obj, k_depth, JS_NewInt32(ctx, event->call.depth));
            } else if (event->type == GUM_EXEC) {
                JS_SetPropertyStr(ctx, obj, k_location,
                                  JS_NewInt64(ctx, (int64_t)(uintptr_t)event->exec.location));
            } else if (event->type == GUM_BLOCK || event->type == GUM_COMPILE) {
                JS_SetPropertyStr(ctx, obj, k_start,
                                  JS_NewInt64(ctx, (int64_t)(uintptr_t)event->block.start));
                JS_SetPropertyStr(ctx, obj, k_end,
                                  JS_NewInt64(ctx, (int64_t)(uintptr_t)event->block.end));
            }
            JS_SetPropertyUint32(ctx, arr, (uint32_t)i, obj);
        }
    }
    return arr;
}

void spindle_stalker_bind(JSContext* ctx) {
    JSValue stalker = JS_NewObject(ctx);
    JSValue global;
    g_on_receive = JS_UNDEFINED;
    g_on_summary = JS_UNDEFINED;
    {
        char follow[sizeof(enc_follow)];
        char unfollow[sizeof(enc_unfollow)];
        char flush[sizeof(enc_flush)];
        char exclude[sizeof(enc_exclude)];
        char parse[sizeof(enc_parse)];
        char disasm[sizeof(enc_disasm)];
        sp_dec(follow, enc_follow, sizeof(enc_follow));
        sp_dec(unfollow, enc_unfollow, sizeof(enc_unfollow));
        sp_dec(flush, enc_flush, sizeof(enc_flush));
        sp_dec(exclude, enc_exclude, sizeof(enc_exclude));
        sp_dec(parse, enc_parse, sizeof(enc_parse));
        sp_dec(disasm, enc_disasm, sizeof(enc_disasm));
        JS_SetPropertyStr(ctx, stalker, follow,
                          JS_NewCFunction(ctx, js_stalker_follow, follow, 2));
        JS_SetPropertyStr(ctx, stalker, unfollow,
                          JS_NewCFunction(ctx, js_stalker_unfollow, unfollow, 1));
        JS_SetPropertyStr(ctx, stalker, flush,
                          JS_NewCFunction(ctx, js_stalker_flush, flush, 0));
        JS_SetPropertyStr(ctx, stalker, exclude,
                          JS_NewCFunction(ctx, js_stalker_exclude, exclude, 2));
        JS_SetPropertyStr(ctx, stalker, parse,
                          JS_NewCFunction(ctx, js_stalker_parse, parse, 1));
        JS_SetPropertyStr(ctx, stalker, disasm,
                          JS_NewCFunction(ctx, js_stalker_disasm, disasm, 1));
    }
    global = JS_GetGlobalObject(ctx);
    {
        char name[sizeof(enc_stalker)];
        sp_dec(name, enc_stalker, sizeof(enc_stalker));
        JS_SetPropertyStr(ctx, global, name, stalker);
    }
    JS_FreeValue(ctx, global);
}
