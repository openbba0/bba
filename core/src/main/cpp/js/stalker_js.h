#pragma once

#include "quickjs.h"

#include <stdint.h>

void spindle_stalker_bind(JSContext* ctx);
int spindle_js_to_u64(JSContext* ctx, JSValueConst val, uint64_t* out);
