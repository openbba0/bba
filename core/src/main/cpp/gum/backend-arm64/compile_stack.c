#include <pthread.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#define COMPILE_STACK_SIZE ((size_t)1 << 20)

static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static pthread_key_t g_key;
static __thread int g_depth;

static void free_stack(void* base) {
    long page = sysconf(_SC_PAGESIZE);
    if (page < 1) {
        page = 4096;
    }
    munmap(base, (size_t)page + COMPILE_STACK_SIZE);
}

static void make_key(void) {
    pthread_key_create(&g_key, free_stack);
}

/* Top of the private stack, 16-byte aligned by the caller. 0 means "already
 * on it" (or mmap failed): the asm must not switch. */
__attribute__((visibility("hidden"))) uintptr_t spindle_compile_stack_enter(void) {
    long page;
    uint8_t* base;

    if (g_depth != 0) {
        return 0;
    }
    pthread_once(&g_once, make_key);
    page = sysconf(_SC_PAGESIZE);
    if (page < 1) {
        page = 4096;
    }
    base = pthread_getspecific(g_key);
    if (base == NULL) {
        base = mmap(NULL, (size_t)page + COMPILE_STACK_SIZE, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) {
            return 0;
        }
        mprotect(base, (size_t)page, PROT_NONE);
        pthread_setspecific(g_key, base);
    }
    g_depth = 1;
    return (uintptr_t)(base + page + COMPILE_STACK_SIZE);
}

__attribute__((visibility("hidden"))) int spindle_compile_stack_busy(void) {
    return g_depth;
}

__attribute__((visibility("hidden"))) void spindle_compile_stack_leave(void) {
    g_depth = 0;
}
