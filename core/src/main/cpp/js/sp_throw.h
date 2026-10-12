#pragma once

#include "quickjs.h"
#include "spindle_str.h"

#define SP_THROW(ctx, enc)                                                     \
    do {                                                                       \
        char _sp_msg[sizeof(enc)];                                             \
        sp_dec(_sp_msg, (enc), sizeof(enc));                                   \
        return JS_ThrowTypeError((ctx), "%s", _sp_msg);                        \
    } while (0)

#define SP_THROW_INTERNAL(ctx, enc)                                            \
    do {                                                                       \
        char _sp_msg[sizeof(enc)];                                             \
        sp_dec(_sp_msg, (enc), sizeof(enc));                                   \
        return JS_ThrowInternalError((ctx), "%s", _sp_msg);                    \
    } while (0)
