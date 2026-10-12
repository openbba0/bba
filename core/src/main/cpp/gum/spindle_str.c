#include "spindle_str.h"

#if defined(__clang__)
#define SP_NOFOLD __attribute__((noinline, optnone, visibility("hidden")))
#else
#define SP_NOFOLD __attribute__((noinline, optimize("O0"), visibility("hidden")))
#endif

SP_NOFOLD void
sp_dec(char *dst, const unsigned char *src, size_t n)
{
    volatile unsigned char key = 0xA7;
    size_t i;

    for (i = 0; i < n; i++)
        dst[i] = (char)(src[i] ^ key);
}
