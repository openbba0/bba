#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*art_hook_enter_cb_t)(void* ctx, void* env, void* args);
typedef void* (*art_hook_leave_cb_t)(void* ctx, void* env, void* retval);
typedef void* (*art_hook_impl_cb_t)(void* ctx, void* env, void* args);

int art_hook_init(void);

/* class_name: dots or slashes. signature NULL = unique declared name. */
int art_hook_attach(const char* class_name, const char* method_name,
                    const char* signature, art_hook_enter_cb_t on_enter,
                    art_hook_leave_cb_t on_leave, art_hook_impl_cb_t impl,
                    void* ctx);

int art_hook_detach(const char* class_name, const char* method_name,
                    const char* signature);

/* Valid only inside an ART hook callback. */
void* art_hook_call_original(void);
void* art_hook_current_env(void);
void* art_hook_jni_env(void);
int art_hook_to_string(void* jobj, char* buf, size_t len);
int art_hook_class_name(void* jobj, char* buf, size_t len);

#define ART_METHOD_MAX_PARAMS 16

struct art_method_info {
    char name[128];
    char sig[512];
    char params[ART_METHOD_MAX_PARAMS][128];
    int argc;
    int is_static;
    int is_ctor;
};

int art_hook_list_methods(const char* class_name, struct art_method_info** out,
                          int* count);
void art_hook_list_free(struct art_method_info* list);

/* params is jobjectArray of user arguments (no this). */
void* art_hook_invoke(const char* class_name, const char* method_name,
                      const char* signature, void* this_jobj, void* params,
                      int is_static);
void* art_hook_new_instance(const char* class_name, const char* signature,
                            void* params);

#ifdef __cplusplus
}
#endif
