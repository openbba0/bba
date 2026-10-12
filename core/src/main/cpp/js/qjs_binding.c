#include "spindle.h"
#include "art_hook.h"
#include "spindle_backtrace.h"
#include "spindle_str.h"
#include "stalker_js.h"

#include "gum.h"
#include "guminterceptor.h"

#include "quickjs.h"
#include "sp_throw.h"
#include "java_use.js.h"

#include <dlfcn.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* Plaintext XOR 0xA7, including the terminating NUL. Decoded on the stack. */
/* "Interceptor" */
static const unsigned char enc_interceptor[] = {
    0xee, 0xc9, 0xd3, 0xc2, 0xd5, 0xc4, 0xc2, 0xd7, 0xd3, 0xc8, 0xd5, 0xa7
};
/* "NativePointer" */
static const unsigned char enc_native_pointer[] = {
    0xe9, 0xc6, 0xd3, 0xce, 0xd1, 0xc2, 0xf7, 0xc8, 0xce, 0xc9, 0xd3, 0xc2, 0xd5, 0xa7
};
/* "InvocationListener" */
static const unsigned char enc_invocation_listener[] = {
    0xee, 0xc9, 0xd1, 0xc8, 0xc4, 0xc6, 0xd3, 0xce, 0xc8, 0xc9, 0xeb, 0xce, 0xd4, 0xd3,
    0xc2, 0xc9, 0xc2, 0xd5, 0xa7
};
/* "Interceptor.attach(addr, callbacks)" */
static const unsigned char enc_interceptor_attach[] = {
    0xee, 0xc9, 0xd3, 0xc2, 0xd5, 0xc4, 0xc2, 0xd7, 0xd3, 0xc8, 0xd5, 0x89, 0xc6, 0xd3,
    0xd3, 0xc6, 0xc4, 0xcf, 0x8f, 0xc6, 0xc3, 0xc3, 0xd5, 0x8b, 0x87, 0xc4, 0xc6, 0xcb,
    0xcb, 0xc5, 0xc6, 0xc4, 0xcc, 0xd4, 0x8e, 0xa7
};
/* "onEnter" */
static const unsigned char enc_on_enter[] = {
    0xc8, 0xc9, 0xe2, 0xc9, 0xd3, 0xc2, 0xd5, 0xa7
};
/* "onLeave" */
static const unsigned char enc_on_leave[] = {
    0xc8, 0xc9, 0xeb, 0xc2, 0xc6, 0xd1, 0xc2, 0xa7
};
/* "onEnter or onLeave required" */
static const unsigned char enc_on_enter_or_leave[] = {
    0xc8, 0xc9, 0xe2, 0xc9, 0xd3, 0xc2, 0xd5, 0x87, 0xc8, 0xd5, 0x87, 0xc8, 0xc9, 0xeb, 0xc2,
    0xc6, 0xd1, 0xc2, 0x87, 0xd5, 0xc2, 0xd6, 0xd2, 0xce, 0xd5, 0xc2, 0xc3, 0xa7
};
/* "onEnter, onLeave, or function required" */
static const unsigned char enc_on_enter_leave_fn[] = {
    0xc8, 0xc9, 0xe2, 0xc9, 0xd3, 0xc2, 0xd5, 0x8b, 0x87, 0xc8, 0xc9, 0xeb, 0xc2, 0xc6, 0xd1,
    0xc2, 0x8b, 0x87, 0xc8, 0xd5, 0x87, 0xc1, 0xd2, 0xc9, 0xc4, 0xd3, 0xce, 0xc8, 0xc9, 0x87,
    0xd5, 0xc2, 0xd6, 0xd2, 0xce, 0xd5, 0xc2, 0xc3, 0xa7
};
/* "detachAll" */
static const unsigned char enc_detach_all[] = {
    0xc3, 0xc2, 0xd3, 0xc6, 0xc4, 0xcf, 0xe6, 0xcb, 0xcb, 0xa7
};
/* "Memory" */
static const unsigned char enc_memory[] = {
    0xea, 0xc2, 0xca, 0xc8, 0xd5, 0xde, 0xa7
};
/* "Module" */
static const unsigned char enc_module[] = {
    0xea, 0xc8, 0xc3, 0xd2, 0xcb, 0xc2, 0xa7
};
/* "Process" */
static const unsigned char enc_process[] = {
    0xf7, 0xd5, 0xc8, 0xc4, 0xc2, 0xd4, 0xd4, 0xa7
};
/* "Backtracer" */
static const unsigned char enc_backtracer[] = {
    0xe5, 0xc6, 0xc4, 0xcc, 0xd3, 0xd5, 0xc6, 0xc4, 0xc2, 0xd5, 0xa7
};
/* "Java" */
static const unsigned char enc_java[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0xa7
};
/* "findExportByName" */
static const unsigned char enc_find_export[] = {
    0xc1, 0xce, 0xc9, 0xc3, 0xe2, 0xdf, 0xd7, 0xc8, 0xd5, 0xd3, 0xe5, 0xde, 0xe9, 0xc6, 0xca,
    0xc2, 0xa7
};
/* "getExportByName" */
static const unsigned char enc_get_export[] = {
    0xc0, 0xc2, 0xd3, 0xe2, 0xdf, 0xd7, 0xc8, 0xd5, 0xd3, 0xe5, 0xde, 0xe9, 0xc6, 0xca, 0xc2,
    0xa7
};
/* "ACCURATE" */
static const unsigned char enc_accurate[] = {
    0xe6, 0xe4, 0xe4, 0xf2, 0xf5, 0xe6, 0xf3, 0xe2, 0xa7
};
/* "FUZZY" */
static const unsigned char enc_fuzzy[] = {
    0xe1, 0xf2, 0xfd, 0xfd, 0xfe, 0xa7
};
/* "getCurrentThreadId" */
static const unsigned char enc_get_tid[] = {
    0xc0, 0xc2, 0xd3, 0xe4, 0xd2, 0xd5, 0xd5, 0xc2, 0xc9, 0xd3, 0xf3, 0xcf, 0xd5, 0xc2, 0xc6, 0xc3,
    0xee, 0xc3, 0xa7
};
/* "returnAddress" */
static const unsigned char enc_return_address[] = {
    0xd5, 0xc2, 0xd3, 0xd2, 0xd5, 0xc9, 0xe6, 0xc3, 0xc3, 0xd5, 0xc2, 0xd4, 0xd4, 0xa7
};
/* "readUtf8String" */
static const unsigned char enc_read_utf8[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf2, 0xd3, 0xc1, 0x9f, 0xf4, 0xd3, 0xd5, 0xce, 0xc9, 0xc0, 0xa7
};
/* "writeUtf8String" */
static const unsigned char enc_write_utf8[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xf2, 0xd3, 0xc1, 0x9f, 0xf4, 0xd3, 0xd5, 0xce, 0xc9, 0xc0, 0xa7
};
/* "readByteArray" */
static const unsigned char enc_read_bytes[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xe5, 0xde, 0xd3, 0xc2, 0xe6, 0xd5, 0xd5, 0xc6, 0xde, 0xa7
};
/* "writeU8" */
static const unsigned char enc_write_u8[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xf2, 0x9f, 0xa7
};
/* "backtrace" */
static const unsigned char enc_backtrace[] = {
    0xc5, 0xc6, 0xc4, 0xcc, 0xd3, 0xd5, 0xc6, 0xc4, 0xc2, 0xa7
};
/* "threadId" */
static const unsigned char enc_thread_id[] = {
    0xd3, 0xcf, 0xd5, 0xc2, 0xc6, 0xc3, 0xee, 0xc3, 0xa7
};
/* "_invoke" */
static const unsigned char enc_invoke[] = {
    0xf8, 0xce, 0xc9, 0xd1, 0xc8, 0xcc, 0xc2, 0xa7
};
/* "_describe" */
static const unsigned char enc_describe[] = {
    0xf8, 0xc3, 0xc2, 0xd4, 0xc4, 0xd5, 0xce, 0xc5, 0xc2, 0xa7
};
/* "_className" */
static const unsigned char enc_class_name[] = {
    0xf8, 0xc4, 0xcb, 0xc6, 0xd4, 0xd4, 0xe9, 0xc6, 0xca, 0xc2, 0xa7
};
/* "readByteArray(length)" */
static const unsigned char enc_read_bytes_len[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xe5, 0xde, 0xd3, 0xc2, 0xe6, 0xd5, 0xd5, 0xc6, 0xde, 0x8f, 0xcb, 0xc2,
    0xc9, 0xc0, 0xd3, 0xcf, 0x8e, 0xa7
};
/* "readUtf8String(length)" */
static const unsigned char enc_read_utf8_len[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf2, 0xd3, 0xc1, 0x9f, 0xf4, 0xd3, 0xd5, 0xce, 0xc9, 0xc0, 0x8f, 0xcb,
    0xc2, 0xc9, 0xc0, 0xd3, 0xcf, 0x8e, 0xa7
};
/* "readPointer" */
static const unsigned char enc_read_pointer[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf7, 0xc8, 0xce, 0xc9, 0xd3, 0xc2, 0xd5, 0xa7
};
/* "writePointer" */
static const unsigned char enc_write_pointer[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xf7, 0xc8, 0xce, 0xc9, 0xd3, 0xc2, 0xd5, 0xa7
};
/* "readCString" */
static const unsigned char enc_read_cstring[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xe4, 0xf4, 0xd3, 0xd5, 0xce, 0xc9, 0xc0, 0xa7
};
/* "writeByteArray" */
static const unsigned char enc_write_byte_array[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xe5, 0xde, 0xd3, 0xc2, 0xe6, 0xd5, 0xd5, 0xc6, 0xde, 0xa7
};
/* "readU8" */
static const unsigned char enc_read_u8[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf2, 0x9f, 0xa7
};
/* "readU16" */
static const unsigned char enc_read_u16[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf2, 0x96, 0x91, 0xa7
};
/* "readU32" */
static const unsigned char enc_read_u32[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf2, 0x94, 0x95, 0xa7
};
/* "readU64" */
static const unsigned char enc_read_u64[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf2, 0x91, 0x93, 0xa7
};
/* "readS8" */
static const unsigned char enc_read_s8[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf4, 0x9f, 0xa7
};
/* "readS16" */
static const unsigned char enc_read_s16[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf4, 0x96, 0x91, 0xa7
};
/* "readS32" */
static const unsigned char enc_read_s32[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf4, 0x94, 0x95, 0xa7
};
/* "readS64" */
static const unsigned char enc_read_s64[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xf4, 0x91, 0x93, 0xa7
};
/* "writeU16" */
static const unsigned char enc_write_u16[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xf2, 0x96, 0x91, 0xa7
};
/* "writeU32" */
static const unsigned char enc_write_u32[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xf2, 0x94, 0x95, 0xa7
};
/* "writeU64" */
static const unsigned char enc_write_u64[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xf2, 0x91, 0x93, 0xa7
};
/* "readFloat" */
static const unsigned char enc_read_float[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xe1, 0xcb, 0xc8, 0xc6, 0xd3, 0xa7
};
/* "readDouble" */
static const unsigned char enc_read_double[] = {
    0xd5, 0xc2, 0xc6, 0xc3, 0xe3, 0xc8, 0xd2, 0xc5, 0xcb, 0xc2, 0xa7
};
/* "writeFloat" */
static const unsigned char enc_write_float[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xe1, 0xcb, 0xc8, 0xc6, 0xd3, 0xa7
};
/* "writeDouble" */
static const unsigned char enc_write_double[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xe3, 0xc8, 0xd2, 0xc5, 0xcb, 0xc2, 0xa7
};
/* "add" */
static const unsigned char enc_add[] = {
    0xc6, 0xc3, 0xc3, 0xa7
};
/* "sub" */
static const unsigned char enc_sub[] = {
    0xd4, 0xd2, 0xc5, 0xa7
};
/* "and" */
static const unsigned char enc_and[] = {
    0xc6, 0xc9, 0xc3, 0xa7
};
/* "or" */
static const unsigned char enc_or[] = {
    0xc8, 0xd5, 0xa7
};
/* "xor" */
static const unsigned char enc_xor[] = {
    0xdf, 0xc8, 0xd5, 0xa7
};
/* "shl" */
static const unsigned char enc_shl[] = {
    0xd4, 0xcf, 0xcb, 0xa7
};
/* "shr" */
static const unsigned char enc_shr[] = {
    0xd4, 0xcf, 0xd5, 0xa7
};
/* "not" */
static const unsigned char enc_not[] = {
    0xc9, 0xc8, 0xd3, 0xa7
};
/* "equals" */
static const unsigned char enc_equals[] = {
    0xc2, 0xd6, 0xd2, 0xc6, 0xcb, 0xd4, 0xa7
};
/* "compare" */
static const unsigned char enc_compare[] = {
    0xc4, 0xc8, 0xca, 0xd7, 0xc6, 0xd5, 0xc2, 0xa7
};
/* "isNull" */
static const unsigned char enc_is_null[] = {
    0xce, 0xd4, 0xe9, 0xd2, 0xcb, 0xcb, 0xa7
};
/* "toInt32" */
static const unsigned char enc_to_int32[] = {
    0xd3, 0xc8, 0xee, 0xc9, 0xd3, 0x94, 0x95, 0xa7
};
/* "valueOf" */
static const unsigned char enc_value_of[] = {
    0xd1, 0xc6, 0xcb, 0xd2, 0xc2, 0xe8, 0xc1, 0xa7
};
/* "toString" */
static const unsigned char enc_to_string[] = {
    0xd3, 0xc8, 0xf4, 0xd3, 0xd5, 0xce, 0xc9, 0xc0, 0xa7
};
/* "alloc" */
static const unsigned char enc_alloc[] = {
    0xc6, 0xcb, 0xcb, 0xc8, 0xc4, 0xa7
};
/* "depth" */
static const unsigned char enc_depth[] = {
    0xc3, 0xc2, 0xd7, 0xd3, 0xcf, 0xa7
};
/* "context" */
static const unsigned char enc_context[] = {
    0xc4, 0xc8, 0xc9, 0xd3, 0xc2, 0xdf, 0xd3, 0xa7
};
/* "replace" */
static const unsigned char enc_replace[] = {
    0xd5, 0xc2, 0xd7, 0xcb, 0xc6, 0xc4, 0xc2, 0xa7
};
/* "detach" */
static const unsigned char enc_detach[] = {
    0xc3, 0xc2, 0xd3, 0xc6, 0xc4, 0xcf, 0xa7
};
/* "attach" */
static const unsigned char enc_attach[] = {
    0xc6, 0xd3, 0xd3, 0xc6, 0xc4, 0xcf, 0xa7
};
/* "flush" */
static const unsigned char enc_flush[] = {
    0xc1, 0xcb, 0xd2, 0xd4, 0xcf, 0xa7
};
/* "ptr" */
static const unsigned char enc_ptr[] = {
    0xd7, 0xd3, 0xd5, 0xa7
};
/* "Thread" */
static const unsigned char enc_thread[] = {
    0xf3, 0xcf, 0xd5, 0xc2, 0xc6, 0xc3, 0xa7
};
/* "hook" */
static const unsigned char enc_hook[] = {
    0xcf, 0xc8, 0xc8, 0xcc, 0xa7
};
/* "unhook" */
static const unsigned char enc_unhook[] = {
    0xd2, 0xc9, 0xcf, 0xc8, 0xc8, 0xcc, 0xa7
};
/* "original" */
static const unsigned char enc_original[] = {
    0xc8, 0xd5, 0xce, 0xc0, 0xce, 0xc9, 0xc6, 0xcb, 0xa7
};
/* "_new" */
static const unsigned char enc_new[] = {
    0xf8, 0xc9, 0xc2, 0xd0, 0xa7
};
/* "rpc" */
static const unsigned char enc_rpc[] = {
    0xd5, 0xd7, 0xc4, 0xa7
};
/* "exports" */
static const unsigned char enc_exports[] = {
    0xc2, 0xdf, 0xd7, 0xc8, 0xd5, 0xd3, 0xd4, 0xa7
};
/* "unlock" */
static const unsigned char enc_unlock[] = {
    0xd2, 0xc9, 0xcb, 0xc8, 0xc4, 0xcc, 0xa7
};
/* "CpuContext" */
static const unsigned char enc_cpu_context[] = {
    0xe4, 0xd7, 0xd2, 0xe4, 0xc8, 0xc9, 0xd3, 0xc2, 0xdf, 0xd3, 0xa7
};
/* "replace(value)" */
static const unsigned char enc_replace_value[] = {
    0xd5, 0xc2, 0xd7, 0xcb, 0xc6, 0xc4, 0xc2, 0x8f, 0xd1, 0xc6, 0xcb, 0xd2, 0xc2, 0x8e, 0xa7
};
/* "invalid address" */
static const unsigned char enc_invalid_address[] = {
    0xce, 0xc9, 0xd1, 0xc6, 0xcb, 0xce, 0xc3, 0x87, 0xc6, 0xc3, 0xc3, 0xd5, 0xc2, 0xd4, 0xd4,
    0xa7
};
/* "attach failed" */
static const unsigned char enc_attach_failed[] = {
    0xc6, 0xd3, 0xd3, 0xc6, 0xc4, 0xcf, 0x87, 0xc1, 0xc6, 0xce, 0xcb, 0xc2, 0xc3, 0xa7
};
/* "class and method required" */
static const unsigned char enc_class_method[] = {
    0xc4, 0xcb, 0xc6, 0xd4, 0xd4, 0x87, 0xc6, 0xc9, 0xc3, 0x87, 0xca, 0xc2, 0xd3, 0xcf, 0xc8,
    0xc3, 0x87, 0xd5, 0xc2, 0xd6, 0xd2, 0xce, 0xd5, 0xc2, 0xc3, 0xa7
};
/* "toString failed" */
static const unsigned char enc_to_string_failed[] = {
    0xd3, 0xc8, 0xf4, 0xd3, 0xd5, 0xce, 0xc9, 0xc0, 0x87, 0xc1, 0xc6, 0xce, 0xcb, 0xc2, 0xc3,
    0xa7
};
/* "not a pointer" */
static const unsigned char enc_not_a_pointer[] = {
    0xc9, 0xc8, 0xd3, 0x87, 0xc6, 0x87, 0xd7, 0xc8, 0xce, 0xc9, 0xd3, 0xc2, 0xd5, 0xa7
};
/* "null pointer" */
static const unsigned char enc_null_pointer[] = {
    0xc9, 0xd2, 0xcb, 0xcb, 0x87, 0xd7, 0xc8, 0xce, 0xc9, 0xd3, 0xc2, 0xd5, 0xa7
};
/* "writeByteArray(ArrayBuffer)" */
static const unsigned char enc_write_bytes_buf[] = {
    0xd0, 0xd5, 0xce, 0xd3, 0xc2, 0xe5, 0xde, 0xd3, 0xc2, 0xe6, 0xd5, 0xd5, 0xc6, 0xde, 0x8f,
    0xe6, 0xd5, 0xd5, 0xc6, 0xde, 0xe5, 0xd2, 0xc1, 0xc1, 0xc2, 0xd5, 0x8e, 0xa7
};
/* "pointer arithmetic" */
static const unsigned char enc_pointer_arith[] = {
    0xd7, 0xc8, 0xce, 0xc9, 0xd3, 0xc2, 0xd5, 0x87, 0xc6, 0xd5, 0xce, 0xd3, 0xcf, 0xca, 0xc2,
    0xd3, 0xce, 0xc4, 0xa7
};
/* "compare(rhs)" */
static const unsigned char enc_compare_rhs[] = {
    0xc4, 0xc8, 0xca, 0xd7, 0xc6, 0xd5, 0xc2, 0x8f, 0xd5, 0xcf, 0xd4, 0x8e, 0xa7
};
/* "toString(radix)" */
static const unsigned char enc_to_string_radix[] = {
    0xd3, 0xc8, 0xf4, 0xd3, 0xd5, 0xce, 0xc9, 0xc0, 0x8f, 0xd5, 0xc6, 0xc3, 0xce, 0xdf, 0x8e,
    0xa7
};
/* "ptr(address)" */
static const unsigned char enc_ptr_address[] = {
    0xd7, 0xd3, 0xd5, 0x8f, 0xc6, 0xc3, 0xc3, 0xd5, 0xc2, 0xd4, 0xd4, 0x8e, 0xa7
};
/* "export not found" */
static const unsigned char enc_export_not_found[] = {
    0xc2, 0xdf, 0xd7, 0xc8, 0xd5, 0xd3, 0x87, 0xc9, 0xc8, 0xd3, 0x87, 0xc1, 0xc8, 0xd2, 0xc9,
    0xc3, 0xa7
};
/* "not a listener" */
static const unsigned char enc_not_a_listener[] = {
    0xc9, 0xc8, 0xd3, 0x87, 0xc6, 0x87, 0xcb, 0xce, 0xd4, 0xd3, 0xc2, 0xc9, 0xc2, 0xd5, 0xa7
};
/* "rpc failed" */
static const unsigned char enc_rpc_failed[] = {
    0xd5, 0xd7, 0xc4, 0x87, 0xc1, 0xc6, 0xce, 0xcb, 0xc2, 0xc3, 0xa7
};
/* "rpc error" */
static const unsigned char enc_rpc_error[] = {
    0xd5, 0xd7, 0xc4, 0x87, 0xc2, 0xd5, 0xd5, 0xc8, 0xd5, 0xa7
};
/* "rpc exception" */
static const unsigned char enc_rpc_exception[] = {
    0xd5, 0xd7, 0xc4, 0x87, 0xc2, 0xdf, 0xc4, 0xc2, 0xd7, 0xd3, 0xce, 0xc8, 0xc9, 0xa7
};
/* "empty rpc" */
static const unsigned char enc_empty_rpc[] = {
    0xc2, 0xca, 0xd7, 0xd3, 0xde, 0x87, 0xd5, 0xd7, 0xc4, 0xa7
};
/* "<rpc>" */
static const unsigned char enc_rpc_file[] = {
    0x9b, 0xd5, 0xd7, 0xc4, 0x99, 0xa7
};
/* "rpc.exports missing" */
static const unsigned char enc_rpc_exports_missing[] = {
    0xd5, 0xd7, 0xc4, 0x89, 0xc2, 0xdf, 0xd7, 0xc8, 0xd5, 0xd3, 0xd4, 0x87, 0xca, 0xce,
    0xd4, 0xd4, 0xce, 0xc9, 0xc0, 0xa7
};
/* "no such export" */
static const unsigned char enc_no_such_export[] = {
    0xc9, 0xc8, 0x87, 0xd4, 0xd2, 0xc4, 0xcf, 0x87, 0xc2, 0xdf, 0xd7, 0xc8, 0xd5, 0xd3, 0xa7
};
/* "Memory.alloc(size)" */
static const unsigned char enc_memory_alloc[] = {
    0xea, 0xc2, 0xca, 0xc8, 0xd5, 0xde, 0x89, 0xc6, 0xcb, 0xcb, 0xc8, 0xc4, 0x8f, 0xd4, 0xce,
    0xdd, 0xc2, 0x8e, 0xa7
};
/* "Memory.alloc failed" */
static const unsigned char enc_memory_alloc_failed[] = {
    0xea, 0xc2, 0xca, 0xc8, 0xd5, 0xde, 0x89, 0xc6, 0xcb, 0xcb, 0xc8, 0xc4, 0x87, 0xc1, 0xc6,
    0xce, 0xcb, 0xc2, 0xc3, 0xa7
};
/* "Module.findExportByName(module, symbol)" */
static const unsigned char enc_module_find_export[] = {
    0xea, 0xc8, 0xc3, 0xd2, 0xcb, 0xc2, 0x89, 0xc1, 0xce, 0xc9, 0xc3, 0xe2, 0xdf, 0xd7, 0xc8,
    0xd5, 0xd3, 0xe5, 0xde, 0xe9, 0xc6, 0xca, 0xc2, 0x8f, 0xca, 0xc8, 0xc3, 0xd2, 0xcb, 0xc2,
    0x8b, 0x87, 0xd4, 0xde, 0xca, 0xc5, 0xc8, 0xcb, 0x8e, 0xa7
};
/* "Thread.backtrace(context, backtracer)" */
static const unsigned char enc_thread_backtrace[] = {
    0xf3, 0xcf, 0xd5, 0xc2, 0xc6, 0xc3, 0x89, 0xc5, 0xc6, 0xc4, 0xcc, 0xd3, 0xd5, 0xc6, 0xc4,
    0xc2, 0x8f, 0xc4, 0xc8, 0xc9, 0xd3, 0xc2, 0xdf, 0xd3, 0x8b, 0x87, 0xc5, 0xc6, 0xc4, 0xcc,
    0xd3, 0xd5, 0xc6, 0xc4, 0xc2, 0xd5, 0x8e, 0xa7
};
/* "Backtracer.ACCURATE or Backtracer.FUZZY" */
static const unsigned char enc_backtracer_kind[] = {
    0xe5, 0xc6, 0xc4, 0xcc, 0xd3, 0xd5, 0xc6, 0xc4, 0xc2, 0xd5, 0x89, 0xe6, 0xe4, 0xe4, 0xf2,
    0xf5, 0xe6, 0xf3, 0xe2, 0x87, 0xc8, 0xd5, 0x87, 0xe5, 0xc6, 0xc4, 0xcc, 0xd3, 0xd5, 0xc6,
    0xc4, 0xc2, 0xd5, 0x89, 0xe1, 0xf2, 0xfd, 0xfd, 0xfe, 0xa7
};
/* "Java.hook(class, method, [sig], callbacks)" */
static const unsigned char enc_java_hook[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xcf, 0xc8, 0xc8, 0xcc, 0x8f, 0xc4, 0xcb, 0xc6, 0xd4, 0xd4,
    0x8b, 0x87, 0xca, 0xc2, 0xd3, 0xcf, 0xc8, 0xc3, 0x8b, 0x87, 0xfc, 0xd4, 0xce, 0xc0, 0xfa,
    0x8b, 0x87, 0xc4, 0xc6, 0xcb, 0xcb, 0xc5, 0xc6, 0xc4, 0xcc, 0xd4, 0x8e, 0xa7
};
/* "Java.hook failed" */
static const unsigned char enc_java_hook_failed[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xcf, 0xc8, 0xc8, 0xcc, 0x87, 0xc1, 0xc6, 0xce, 0xcb, 0xc2,
    0xc3, 0xa7
};
/* "Java.unhook(class, method, [sig])" */
static const unsigned char enc_java_unhook[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xd2, 0xc9, 0xcf, 0xc8, 0xc8, 0xcc, 0x8f, 0xc4, 0xcb, 0xc6,
    0xd4, 0xd4, 0x8b, 0x87, 0xca, 0xc2, 0xd3, 0xcf, 0xc8, 0xc3, 0x8b, 0x87, 0xfc, 0xd4, 0xce,
    0xc0, 0xfa, 0x8e, 0xa7
};
/* "Java.unhook failed" */
static const unsigned char enc_java_unhook_failed[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xd2, 0xc9, 0xcf, 0xc8, 0xc8, 0xcc, 0x87, 0xc1, 0xc6, 0xce,
    0xcb, 0xc2, 0xc3, 0xa7
};
/* "Java.original only inside a hook" */
static const unsigned char enc_java_original[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xc8, 0xd5, 0xce, 0xc0, 0xce, 0xc9, 0xc6, 0xcb, 0x87, 0xc8,
    0xc9, 0xcb, 0xde, 0x87, 0xce, 0xc9, 0xd4, 0xce, 0xc3, 0xc2, 0x87, 0xc6, 0x87, 0xcf, 0xc8,
    0xc8, 0xcc, 0xa7
};
/* "Java.str(obj)" */
static const unsigned char enc_java_str[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xd4, 0xd3, 0xd5, 0x8f, 0xc8, 0xc5, 0xcd, 0x8e, 0xa7
};
/* "Java.str expects a Java handle" */
static const unsigned char enc_java_str_handle[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xd4, 0xd3, 0xd5, 0x87, 0xc2, 0xdf, 0xd7, 0xc2, 0xc4, 0xd3,
    0xd4, 0x87, 0xc6, 0x87, 0xed, 0xc6, 0xd1, 0xc6, 0x87, 0xcf, 0xc6, 0xc9, 0xc3, 0xcb, 0xc2,
    0xa7
};
/* "Java._describe(class)" */
static const unsigned char enc_java_describe[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xf8, 0xc3, 0xc2, 0xd4, 0xc4, 0xd5, 0xce, 0xc5, 0xc2, 0x8f,
    0xc4, 0xcb, 0xc6, 0xd4, 0xd4, 0x8e, 0xa7
};
/* "Java.use: class not found" */
static const unsigned char enc_java_use_missing[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xd2, 0xd4, 0xc2, 0x9d, 0x87, 0xc4, 0xcb, 0xc6, 0xd4, 0xd4,
    0x87, 0xc9, 0xc8, 0xd3, 0x87, 0xc1, 0xc8, 0xd2, 0xc9, 0xc3, 0xa7
};
/* "Java._invoke(class, name, sig, this, args)" */
static const unsigned char enc_java_invoke[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xf8, 0xce, 0xc9, 0xd1, 0xc8, 0xcc, 0xc2, 0x8f, 0xc4, 0xcb,
    0xc6, 0xd4, 0xd4, 0x8b, 0x87, 0xc9, 0xc6, 0xca, 0xc2, 0x8b, 0x87, 0xd4, 0xce, 0xc0, 0x8b,
    0x87, 0xd3, 0xcf, 0xce, 0xd4, 0x8b, 0x87, 0xc6, 0xd5, 0xc0, 0xd4, 0x8e, 0xa7
};
/* "Java._new(class, sig, args)" */
static const unsigned char enc_java_new[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xf8, 0xc9, 0xc2, 0xd0, 0x8f, 0xc4, 0xcb, 0xc6, 0xd4, 0xd4,
    0x8b, 0x87, 0xd4, 0xce, 0xc0, 0x8b, 0x87, 0xc6, 0xd5, 0xc0, 0xd4, 0x8e, 0xa7
};
/* "Java._className(obj)" */
static const unsigned char enc_java_class_name[] = {
    0xed, 0xc6, 0xd1, 0xc6, 0x89, 0xf8, 0xc4, 0xcb, 0xc6, 0xd4, 0xd4, 0xe9, 0xc6, 0xca, 0xc2,
    0x8f, 0xc8, 0xc5, 0xcd, 0x8e, 0xa7
};
/* "<java.use>" */
static const unsigned char enc_java_use_name[] = {
    0x9b, 0xcd, 0xc6, 0xd1, 0xc6, 0x89, 0xd2, 0xd4, 0xc2, 0x99, 0xa7
};
/* "enumerateModules" */
static const unsigned char enc_enumerate_modules[] = {
    0xc2, 0xc9, 0xd2, 0xca, 0xc2, 0xd5, 0xc6, 0xd3, 0xc2, 0xea, 0xc8, 0xc3, 0xd2, 0xcb, 0xc2, 0xd4, 0xa7
};
/* "enumerateExports" */
static const unsigned char enc_enumerate_exports[] = {
    0xc2, 0xc9, 0xd2, 0xca, 0xc2, 0xd5, 0xc6, 0xd3, 0xc2, 0xe2, 0xdf, 0xd7, 0xc8, 0xd5, 0xd3, 0xd4, 0xa7
};
/* "enumerateImports" */
static const unsigned char enc_enumerate_imports[] = {
    0xc2, 0xc9, 0xd2, 0xca, 0xc2, 0xd5, 0xc6, 0xd3, 0xc2, 0xee, 0xca, 0xd7, 0xc8, 0xd5, 0xd3, 0xd4, 0xa7
};
/* "load" */
static const unsigned char enc_load[] = {
    0xcb, 0xc8, 0xc6, 0xc3, 0xa7
};
/* "findBaseAddress" */
static const unsigned char enc_find_base_address[] = {
    0xc1, 0xce, 0xc9, 0xc3, 0xe5, 0xc6, 0xd4, 0xc2, 0xe6, 0xc3, 0xc3, 0xd5, 0xc2, 0xd4, 0xd4, 0xa7
};
/* "getModuleByName" */
static const unsigned char enc_get_module_by_name[] = {
    0xc0, 0xc2, 0xd3, 0xea, 0xc8, 0xc3, 0xd2, 0xcb, 0xc2, 0xe5, 0xde, 0xe9, 0xc6, 0xca, 0xc2, 0xa7
};
/* "findModuleByName" */
static const unsigned char enc_find_module_by_name[] = {
    0xc1, 0xce, 0xc9, 0xc3, 0xea, 0xc8, 0xc3, 0xd2, 0xcb, 0xc2, 0xe5, 0xde, 0xe9, 0xc6, 0xca, 0xc2, 0xa7
};
/* "findModuleByAddress" */
static const unsigned char enc_find_module_by_address[] = {
    0xc1, 0xce, 0xc9, 0xc3, 0xea, 0xc8, 0xc3, 0xd2, 0xcb, 0xc2, 0xe5, 0xde, 0xe6, 0xc3, 0xc3, 0xd5, 0xc2, 0xd4, 0xd4, 0xa7
};
/* "enumerateThreads" */
static const unsigned char enc_enumerate_threads[] = {
    0xc2, 0xc9, 0xd2, 0xca, 0xc2, 0xd5, 0xc6, 0xd3, 0xc2, 0xf3, 0xcf, 0xd5, 0xc2, 0xc6, 0xc3, 0xd4, 0xa7
};
/* "isDebuggerAttached" */
static const unsigned char enc_is_debugger_attached[] = {
    0xce, 0xd4, 0xe3, 0xc2, 0xc5, 0xd2, 0xc0, 0xc0, 0xc2, 0xd5, 0xe6, 0xd3, 0xd3, 0xc6, 0xc4, 0xcf, 0xc2, 0xc3, 0xa7
};
/* "arch" */
static const unsigned char enc_arch[] = {
    0xc6, 0xd5, 0xc4, 0xcf, 0xa7
};
/* "platform" */
static const unsigned char enc_platform[] = {
    0xd7, 0xcb, 0xc6, 0xd3, 0xc1, 0xc8, 0xd5, 0xca, 0xa7
};
/* "pageSize" */
static const unsigned char enc_page_size[] = {
    0xd7, 0xc6, 0xc0, 0xc2, 0xf4, 0xce, 0xdd, 0xc2, 0xa7
};
/* "pointerSize" */
static const unsigned char enc_pointer_size[] = {
    0xd7, 0xc8, 0xce, 0xc9, 0xd3, 0xc2, 0xd5, 0xf4, 0xce, 0xdd, 0xc2, 0xa7
};
/* "codeSigningPolicy" */
static const unsigned char enc_code_signing_policy[] = {
    0xc4, 0xc8, 0xc3, 0xc2, 0xf4, 0xce, 0xc0, 0xc9, 0xce, 0xc9, 0xc0, 0xf7, 0xc8, 0xcb, 0xce, 0xc4, 0xde, 0xa7
};

static pthread_mutex_t g_js_mu;
static pthread_once_t g_js_once = PTHREAD_ONCE_INIT;
static JSRuntime* g_rt = NULL;
static JSContext* g_ctx = NULL;
static int g_in_js = 0;
static int g_js_depth = 0;

struct hook_js_ctx {
    JSValue on_enter;
    JSValue on_leave;
    JSValue impl;
    int in_impl;
    int depth;
};

struct art_js_entry {
    char* class_name;
    char* method_name;
    char* signature;
    struct hook_js_ctx* hook;
    struct art_js_entry* next;
};

static struct art_js_entry* g_art_js;

static void init_js_mu(void) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_js_mu, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void lock_js(void) {
    pthread_once(&g_js_once, init_js_mu);
    pthread_mutex_lock(&g_js_mu);
}

static void unlock_js(void) {
    pthread_mutex_unlock(&g_js_mu);
}

/* Caller holds g_js_mu. stack_top is recorded only on the outermost entry;
 * another thread's SP then looks like an overflow. Nested calls (a Java
 * hook firing inside a running script) keep the existing top. */
static void enter_js(void) {
    if (g_js_depth == 0 && g_rt != NULL) {
        JS_UpdateStackTop(g_rt);
    }
    g_js_depth++;
    g_in_js = 1;
}

static void leave_js(void) {
    if (g_js_depth > 0) {
        g_js_depth--;
    }
    g_in_js = g_js_depth > 0;
}

static JSClassID g_ptr_class;
static JSClassID g_listener_class;
static JSClassID g_cpu_class;
static JSClassID g_module_class;

struct listener_node {
    void* listener;
    struct listener_node* next;
};

static struct listener_node* g_listeners;

static uint64_t* ptr_opaque(JSValueConst val) {
    if (g_ptr_class == 0) {
        return NULL;
    }
    return JS_GetOpaque(val, g_ptr_class);
}

static int parse_u64(JSContext* ctx, JSValueConst val, uint64_t* out) {
    int64_t v = 0;
    const char* s;
    char* end = NULL;
    uint64_t* p = ptr_opaque(val);
    if (p != NULL) {
        *out = *p;
        return 0;
    }
    if (JS_IsString(val)) {
        s = JS_ToCString(ctx, val);
        if (s == NULL) {
            return -1;
        }
        *out = strtoull(s, &end, 0);
        JS_FreeCString(ctx, s);
        return 0;
    }
    if (JS_ToInt64Ext(ctx, &v, val) != 0) {
        return -1;
    }
    *out = (uint64_t)v;
    return 0;
}

int spindle_js_to_u64(JSContext* ctx, JSValueConst val, uint64_t* out) {
    return parse_u64(ctx, val, out);
}

static JSValue js_rpc_unlock(JSContext* ctx, JSValueConst this_val, int argc,
                             JSValueConst* argv) {
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    spindle_set_unlock();
    return JS_UNDEFINED;
}

static void log_cstr(const char* s) {
    if (s) {
        //__android_log_print(ANDROID_LOG_INFO, "SPDL", "%s", s);
        spindle_log(s);
    }
}

static JSValue js_console_log(JSContext* ctx, JSValueConst this_val, int argc,
                              JSValueConst* argv) {
    int i;
    (void)this_val;
    for (i = 0; i < argc; i++) {
        const char* s = JS_ToCString(ctx, argv[i]);
        if (s) {
            log_cstr(s);
            JS_FreeCString(ctx, s);
        }
    }
    return JS_UNDEFINED;
}

static void log_js_exception(void) {
    JSValue ex = JS_GetException(g_ctx);
    const char* s = JS_ToCString(g_ctx, ex);
    log_cstr(s ? s : "hook exception");
    if (s) {
        JS_FreeCString(g_ctx, s);
    }
    JS_FreeValue(g_ctx, ex);
}

static void call_js_hook(JSValue fn, JSValue this_obj, JSValue arg) {
    JSValue argv[1];
    JSValue ret;
    argv[0] = arg;
    ret = JS_Call(g_ctx, fn, this_obj, 1, argv);
    if (JS_IsException(ret)) {
        log_js_exception();
    } else {
        JS_FreeValue(g_ctx, ret);
    }
}

static JSValue js_retval_replace(JSContext* ctx, JSValueConst this_val, int argc,
                                 JSValueConst* argv) {
    if (argc < 1) {
        SP_THROW(ctx, enc_replace_value);
    }
    if (JS_SetPropertyUint32(ctx, this_val, 0, JS_DupValue(ctx, argv[0])) < 0) {
        return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}

static JSValue js_ptr_new(JSContext* ctx, uint64_t addr);

static JSValue js_cpu_new(JSContext* ctx, const struct spindle_cpu_snap* snap);

static void set_prop_enc(JSContext* ctx, JSValue obj, const unsigned char* enc, size_t n,
                         JSValue v);
static void set_cfunc_enc(JSContext* ctx, JSValue obj, const unsigned char* enc, size_t n,
                          JSCFunction* fn, int argc);

static void hook_on_enter(void* ctx, uint64_t gprs[8], uint64_t ret_addr, uint32_t tid,
                          int depth, int has_meta, const struct spindle_cpu_snap* snap) {
    struct hook_js_ctx* hook = ctx;
    JSValue args;
    JSValue thiz;
    int i;
    if (hook == NULL || g_ctx == NULL || !JS_IsFunction(g_ctx, hook->on_enter)) {
        return;
    }
    lock_js();
    if (g_in_js) {
        unlock_js();
        return;
    }
    enter_js();
    args = JS_NewArray(g_ctx);
    for (i = 0; i < 8; i++) {
        JS_SetPropertyUint32(g_ctx, args, (uint32_t)i, js_ptr_new(g_ctx, gprs[i]));
    }
    thiz = JS_UNDEFINED;
    if (has_meta) {
        thiz = JS_NewObject(g_ctx);
        {
            char ret_name[sizeof(enc_return_address)];
            char tid_name[sizeof(enc_thread_id)];
            sp_dec(ret_name, enc_return_address, sizeof(enc_return_address));
            sp_dec(tid_name, enc_thread_id, sizeof(enc_thread_id));
            JS_SetPropertyStr(g_ctx, thiz, ret_name, js_ptr_new(g_ctx, ret_addr));
            JS_SetPropertyStr(g_ctx, thiz, tid_name, JS_NewInt32(g_ctx, (int32_t)tid));
        }
        set_prop_enc(g_ctx, thiz, enc_depth, sizeof(enc_depth), JS_NewInt32(g_ctx, depth));
        if (snap != NULL) {
            set_prop_enc(g_ctx, thiz, enc_context, sizeof(enc_context),
                         js_cpu_new(g_ctx, snap));
        }
    }
    call_js_hook(hook->on_enter, thiz, args);
    for (i = 0; i < 8; i++) {
        JSValue v = JS_GetPropertyUint32(g_ctx, args, (uint32_t)i);
        uint64_t x;
        if (parse_u64(g_ctx, v, &x) == 0) {
            gprs[i] = x;
        }
        JS_FreeValue(g_ctx, v);
    }
    JS_FreeValue(g_ctx, args);
    JS_FreeValue(g_ctx, thiz);
    leave_js();
    unlock_js();
}

static void hook_on_leave(void* ctx, uint64_t retval[2], uint64_t ret_addr, uint32_t tid,
                          int depth, int has_meta, const struct spindle_cpu_snap* snap) {
    struct hook_js_ctx* hook = ctx;
    JSValue args;
    JSValue v0;
    JSValue v1;
    uint64_t x;
    if (hook == NULL || g_ctx == NULL) {
        return;
    }
    lock_js();
    if (g_in_js) {
        unlock_js();
        return;
    }
    enter_js();
    args = JS_NewArray(g_ctx);
    JS_SetPropertyUint32(g_ctx, args, 0, js_ptr_new(g_ctx, retval[0]));
    JS_SetPropertyUint32(g_ctx, args, 1, js_ptr_new(g_ctx, retval[1]));
    set_cfunc_enc(g_ctx, args, enc_replace, sizeof(enc_replace), js_retval_replace, 1);
    {
        JSValue thiz = JS_UNDEFINED;
        if (has_meta) {
            thiz = JS_NewObject(g_ctx);
            {
                char ret_name[sizeof(enc_return_address)];
                char tid_name[sizeof(enc_thread_id)];
                sp_dec(ret_name, enc_return_address, sizeof(enc_return_address));
                sp_dec(tid_name, enc_thread_id, sizeof(enc_thread_id));
                JS_SetPropertyStr(g_ctx, thiz, ret_name, js_ptr_new(g_ctx, ret_addr));
                JS_SetPropertyStr(g_ctx, thiz, tid_name, JS_NewInt32(g_ctx, (int32_t)tid));
            }
            set_prop_enc(g_ctx, thiz, enc_depth, sizeof(enc_depth), JS_NewInt32(g_ctx, depth));
            if (snap != NULL) {
                set_prop_enc(g_ctx, thiz, enc_context, sizeof(enc_context),
                             js_cpu_new(g_ctx, snap));
            }
        }
        call_js_hook(hook->on_leave, thiz, args);
        JS_FreeValue(g_ctx, thiz);
    }
    v0 = JS_GetPropertyUint32(g_ctx, args, 0);
    v1 = JS_GetPropertyUint32(g_ctx, args, 1);
    if (parse_u64(g_ctx, v0, &x) == 0) {
        retval[0] = x;
    }
    if (parse_u64(g_ctx, v1, &x) == 0) {
        retval[1] = x;
    }
    JS_FreeValue(g_ctx, v0);
    JS_FreeValue(g_ctx, v1);
    JS_FreeValue(g_ctx, args);
    leave_js();
    unlock_js();
}

extern int spindle_compile_stack_busy(void);

static GumInterceptor* g_gum_interceptor;

static void ensure_gum_interceptor(void) {
    gum_init();
    if (g_gum_interceptor == NULL) {
        g_gum_interceptor = gum_interceptor_obtain();
    }
}

static void fill_snap(const GumCpuContext* cpu, struct spindle_cpu_snap* snap) {
    snap->pc = cpu->pc;
    snap->sp = cpu->sp;
    snap->fp = cpu->fp;
    snap->lr = cpu->lr;
    memcpy(snap->x, cpu->x, sizeof(snap->x));
}

static void gum_hook_enter(GumInvocationContext* ic, gpointer user_data) {
    uint64_t gprs[8];
    struct spindle_cpu_snap snap;
    guint i;
    if (spindle_compile_stack_busy()) {
        return;
    }
    for (i = 0; i < 8; i++) {
        gprs[i] = ic->cpu_context->x[i];
    }
    fill_snap(ic->cpu_context, &snap);
    hook_on_enter(user_data, gprs,
                  (uint64_t)(uintptr_t)gum_invocation_context_get_return_address(ic),
                  gum_invocation_context_get_thread_id(ic),
                  (int)gum_invocation_context_get_depth(ic), 1, &snap);
    for (i = 0; i < 8; i++) {
        ic->cpu_context->x[i] = gprs[i];
    }
}

static void gum_hook_leave(GumInvocationContext* ic, gpointer user_data) {
    uint64_t rv[2];
    struct spindle_cpu_snap snap;
    if (spindle_compile_stack_busy()) {
        return;
    }
    rv[0] = ic->cpu_context->x[0];
    rv[1] = ic->cpu_context->x[1];
    fill_snap(ic->cpu_context, &snap);
    hook_on_leave(user_data, rv,
                  (uint64_t)(uintptr_t)gum_invocation_context_get_return_address(ic),
                  gum_invocation_context_get_thread_id(ic),
                  (int)gum_invocation_context_get_depth(ic), 1, &snap);
    ic->cpu_context->x[0] = rv[0];
    ic->cpu_context->x[1] = rv[1];
}

static int resolve_export(const char* spec, uint64_t* out) {
    const char* bang = strchr(spec, '!');
    char lib[128];
    size_t n;
    void* handle;
    void* sym;
    if (bang == NULL || bang == spec || bang[1] == '\0') {
        return -1;
    }
    n = (size_t)(bang - spec);
    if (n >= sizeof(lib)) {
        return -1;
    }
    memcpy(lib, spec, n);
    lib[n] = '\0';
    handle = dlopen(lib, RTLD_NOW | RTLD_NOLOAD);
    if (handle == NULL) {
        handle = dlopen(lib, RTLD_NOW);
    }
    if (handle == NULL) {
        return -1;
    }
    sym = dlsym(handle, bang + 1);
    if (sym == NULL) {
        return -1;
    }
    *out = (uint64_t)(uintptr_t)sym;
    return 0;
}

static JSValue js_interceptor_attach(JSContext* ctx, JSValueConst this_val, int argc,
                                     JSValueConst* argv) {
    uint64_t addr = 0;
    JSValue on_enter;
    JSValue on_leave;
    struct hook_js_ctx* hook;
    (void)this_val;
    if (argc < 2) {
        char msg[sizeof(enc_interceptor_attach)];
        sp_dec(msg, enc_interceptor_attach, sizeof(enc_interceptor_attach));
        return JS_ThrowTypeError(ctx, "%s", msg);
    }
    if (JS_IsString(argv[0])) {
        const char* spec = JS_ToCString(ctx, argv[0]);
        int rc = -1;
        if (spec != NULL && strchr(spec, '!') != NULL) {
            rc = resolve_export(spec, &addr);
        }
        if (spec != NULL) {
            JS_FreeCString(ctx, spec);
        }
        if (rc != 0 && parse_u64(ctx, argv[0], &addr) != 0) {
            SP_THROW(ctx, enc_invalid_address);
        }
    } else if (parse_u64(ctx, argv[0], &addr) != 0) {
        SP_THROW(ctx, enc_invalid_address);
    }
    if (addr == 0) {
        SP_THROW(ctx, enc_invalid_address);
    }
    {
        char enter_name[sizeof(enc_on_enter)];
        char leave_name[sizeof(enc_on_leave)];
        sp_dec(enter_name, enc_on_enter, sizeof(enc_on_enter));
        sp_dec(leave_name, enc_on_leave, sizeof(enc_on_leave));
        on_enter = JS_GetPropertyStr(ctx, argv[1], enter_name);
        on_leave = JS_GetPropertyStr(ctx, argv[1], leave_name);
    }
    if (!JS_IsFunction(ctx, on_enter)) {
        JS_FreeValue(ctx, on_enter);
        on_enter = JS_UNDEFINED;
        if (JS_IsFunction(ctx, argv[1])) {
            on_enter = JS_DupValue(ctx, argv[1]);
            JS_FreeValue(ctx, on_leave);
            on_leave = JS_UNDEFINED;
        }
    }
    if (!JS_IsFunction(ctx, on_leave)) {
        JS_FreeValue(ctx, on_leave);
        on_leave = JS_UNDEFINED;
    }
    if (!JS_IsFunction(ctx, on_enter) && !JS_IsFunction(ctx, on_leave)) {
        JS_FreeValue(ctx, on_enter);
        JS_FreeValue(ctx, on_leave);
        SP_THROW(ctx, enc_on_enter_or_leave);
    }
    hook = malloc(sizeof(*hook));
    if (hook == NULL) {
        JS_FreeValue(ctx, on_enter);
        JS_FreeValue(ctx, on_leave);
        return JS_ThrowOutOfMemory(ctx);
    }
    hook->on_enter = on_enter;
    hook->on_leave = on_leave;
    hook->impl = JS_UNDEFINED;
    hook->in_impl = 0;
    hook->depth = 0;
    {
        int have_enter = JS_IsFunction(ctx, on_enter);
        int have_leave = JS_IsFunction(ctx, on_leave);
        GumInvocationListener* listener;
        GumAttachReturn attached;
        ensure_gum_interceptor();
        if (have_leave) {
            listener = gum_make_call_listener(have_enter ? gum_hook_enter : NULL,
                                              gum_hook_leave, hook, NULL);
        } else {
            listener = gum_make_probe_listener(gum_hook_enter, hook, NULL);
        }
        attached = gum_interceptor_attach(g_gum_interceptor,
                                          (gpointer)(uintptr_t)addr, listener, NULL);
        if (attached != GUM_ATTACH_OK) {
            g_object_unref(listener);
            JS_FreeValue(ctx, hook->on_enter);
            JS_FreeValue(ctx, hook->on_leave);
            JS_FreeValue(ctx, hook->impl);
            free(hook);
            SP_THROW_INTERNAL(ctx, enc_attach_failed);
        }
        {
            struct listener_node* node = malloc(sizeof(*node));
            JSValue obj;
            if (node == NULL) {
                g_object_unref(listener);
                return JS_ThrowOutOfMemory(ctx);
            }
            node->listener = listener;
            node->next = g_listeners;
            g_listeners = node;
            obj = JS_NewObjectClass(ctx, (int)g_listener_class);
            if (JS_IsException(obj)) {
                return obj;
            }
            JS_SetOpaque(obj, node);
            return obj;
        }
    }
    return JS_UNDEFINED;
}

static JSValue call_js_hook_ret(JSValue fn, JSValue arg) {
    JSValue argv[1];
    JSValue ret;
    argv[0] = arg;
    ret = JS_Call(g_ctx, fn, JS_UNDEFINED, 1, argv);
    if (JS_IsException(ret)) {
        log_js_exception();
        return JS_UNDEFINED;
    }
    return ret;
}

static JSValue jstring_to_js(JNIEnv* env, jstring s) {
    const char* utf;
    JSValue v;
    if (s == NULL) {
        return JS_NULL;
    }
    utf = (*env)->GetStringUTFChars(env, s, NULL);
    if (utf == NULL) {
        return JS_NULL;
    }
    v = JS_NewString(g_ctx, utf);
    (*env)->ReleaseStringUTFChars(env, s, utf);
    return v;
}

/* Method.invoke boxes primitives. kind: I J S B Z F D C */
static int unbox_primitive(JNIEnv* env, jobject obj, const char* cls_name,
                           const char* method, const char* sig, int kind, JSValue* out) {
    jclass cl;
    jmethodID mid;
    cl = (*env)->FindClass(env, cls_name);
    if (cl == NULL) {
        (*env)->ExceptionClear(env);
        return 0;
    }
    if (!(*env)->IsInstanceOf(env, obj, cl)) {
        (*env)->DeleteLocalRef(env, cl);
        return 0;
    }
    mid = (*env)->GetMethodID(env, cl, method, sig);
    (*env)->DeleteLocalRef(env, cl);
    if (mid == NULL) {
        (*env)->ExceptionClear(env);
        return 0;
    }
    switch (kind) {
    case 'I': {
        jint v = (*env)->CallIntMethod(env, obj, mid);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            return 0;
        }
        *out = JS_NewInt32(g_ctx, v);
        return 1;
    }
    case 'J': {
        jlong v = (*env)->CallLongMethod(env, obj, mid);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            return 0;
        }
        *out = JS_NewInt64(g_ctx, (int64_t)v);
        return 1;
    }
    case 'S': {
        jshort v = (*env)->CallShortMethod(env, obj, mid);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            return 0;
        }
        *out = JS_NewInt32(g_ctx, (int32_t)v);
        return 1;
    }
    case 'B': {
        jbyte v = (*env)->CallByteMethod(env, obj, mid);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            return 0;
        }
        *out = JS_NewInt32(g_ctx, (int32_t)v);
        return 1;
    }
    case 'Z': {
        jboolean v = (*env)->CallBooleanMethod(env, obj, mid);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            return 0;
        }
        *out = JS_NewBool(g_ctx, v ? 1 : 0);
        return 1;
    }
    case 'F': {
        jfloat v = (*env)->CallFloatMethod(env, obj, mid);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            return 0;
        }
        *out = JS_NewFloat64(g_ctx, (double)v);
        return 1;
    }
    case 'D': {
        jdouble v = (*env)->CallDoubleMethod(env, obj, mid);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            return 0;
        }
        *out = JS_NewFloat64(g_ctx, (double)v);
        return 1;
    }
    case 'C': {
        jchar v = (*env)->CallCharMethod(env, obj, mid);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
            return 0;
        }
        *out = JS_NewInt32(g_ctx, (int32_t)v);
        return 1;
    }
    default:
        return 0;
    }
}

static JSValue jobject_to_js(JNIEnv* env, jobject obj) {
    jclass str_cl;
    JSValue v;
    if (obj == NULL) {
        return JS_NULL;
    }
    str_cl = (*env)->FindClass(env, "java/lang/String");
    if (str_cl != NULL && (*env)->IsInstanceOf(env, obj, str_cl)) {
        v = jstring_to_js(env, (jstring)obj);
        (*env)->DeleteLocalRef(env, str_cl);
        return v;
    }
    if (str_cl != NULL) {
        (*env)->DeleteLocalRef(env, str_cl);
    }
    if (unbox_primitive(env, obj, "java/lang/Integer", "intValue", "()I", 'I', &v) ||
        unbox_primitive(env, obj, "java/lang/Long", "longValue", "()J", 'J', &v) ||
        unbox_primitive(env, obj, "java/lang/Boolean", "booleanValue", "()Z", 'Z', &v) ||
        unbox_primitive(env, obj, "java/lang/Short", "shortValue", "()S", 'S', &v) ||
        unbox_primitive(env, obj, "java/lang/Byte", "byteValue", "()B", 'B', &v) ||
        unbox_primitive(env, obj, "java/lang/Float", "floatValue", "()F", 'F', &v) ||
        unbox_primitive(env, obj, "java/lang/Double", "doubleValue", "()D", 'D', &v) ||
        unbox_primitive(env, obj, "java/lang/Character", "charValue", "()C", 'C', &v)) {
        return v;
    }
    return JS_NewBigUint64(g_ctx, (uint64_t)(uintptr_t)obj);
}

static JSValue args_to_js(JNIEnv* env, jobjectArray arr) {
    JSValue a = JS_NewArray(g_ctx);
    jsize n = arr != NULL ? (*env)->GetArrayLength(env, arr) : 0;
    jsize i;
    for (i = 0; i < n; i++) {
        jobject o = (*env)->GetObjectArrayElement(env, arr, i);
        JS_SetPropertyUint32(g_ctx, a, (uint32_t)i, jobject_to_js(env, o));
    }
    return a;
}

static jobject js_to_jobject(JNIEnv* env, JSValueConst v) {
    int64_t n = 0;
    uint64_t u = 0;
    if (JS_IsNull(v) || JS_IsUndefined(v)) {
        return NULL;
    }
    if (JS_IsBool(v)) {
        jclass c = (*env)->FindClass(env, "java/lang/Boolean");
        jmethodID m = (*env)->GetStaticMethodID(env, c, "valueOf",
                                               "(Z)Ljava/lang/Boolean;");
        jobject o = (*env)->CallStaticObjectMethod(env, c, m, JS_ToBool(g_ctx, v));
        (*env)->DeleteLocalRef(env, c);
        return o;
    }
    if (JS_IsString(v)) {
        const char* s = JS_ToCString(g_ctx, v);
        jobject o = (*env)->NewStringUTF(env, s ? s : "");
        if (s) {
            JS_FreeCString(g_ctx, s);
        }
        return o;
    }
    if (JS_IsNumber(v)) {
        jclass c;
        jmethodID m;
        jobject o;
        if (JS_ToInt64Ext(g_ctx, &n, v) != 0) {
            return NULL;
        }
        if (n >= (int64_t)INT32_MIN && n <= (int64_t)INT32_MAX) {
            c = (*env)->FindClass(env, "java/lang/Integer");
            m = (*env)->GetStaticMethodID(env, c, "valueOf", "(I)Ljava/lang/Integer;");
            o = (*env)->CallStaticObjectMethod(env, c, m, (jint)n);
        } else {
            c = (*env)->FindClass(env, "java/lang/Long");
            m = (*env)->GetStaticMethodID(env, c, "valueOf", "(J)Ljava/lang/Long;");
            o = (*env)->CallStaticObjectMethod(env, c, m, (jlong)n);
        }
        (*env)->DeleteLocalRef(env, c);
        return o;
    }
    if (JS_IsObject(v)) {
        JSValue h = JS_GetPropertyStr(g_ctx, v, "$h");
        if (!JS_IsUndefined(h) && !JS_IsNull(h)) {
            jobject o = js_to_jobject(env, h);
            JS_FreeValue(g_ctx, h);
            return o;
        }
        JS_FreeValue(g_ctx, h);
    }
    if (parse_u64(g_ctx, v, &u) == 0) {
        return (jobject)(uintptr_t)u;
    }
    return NULL;
}

static jobjectArray js_array_to_jobjects(JNIEnv* env, JSValueConst arr) {
    JSValue lenv;
    int64_t n = 0;
    jobjectArray out;
    int64_t i;
    jclass obj_cl;
    if (!JS_IsObject(arr)) {
        return NULL;
    }
    lenv = JS_GetPropertyStr(g_ctx, arr, "length");
    if (JS_ToInt64(g_ctx, &n, lenv) != 0) {
        n = 0;
    }
    JS_FreeValue(g_ctx, lenv);
    obj_cl = (*env)->FindClass(env, "java/lang/Object");
    if (obj_cl == NULL) {
        return NULL;
    }
    out = (*env)->NewObjectArray(env, (jsize)n, obj_cl, NULL);
    (*env)->DeleteLocalRef(env, obj_cl);
    if (out == NULL) {
        return NULL;
    }
    for (i = 0; i < n; i++) {
        JSValue v = JS_GetPropertyUint32(g_ctx, arr, (uint32_t)i);
        jobject o = js_to_jobject(env, v);
        (*env)->SetObjectArrayElement(env, out, (jsize)i, o);
        JS_FreeValue(g_ctx, v);
    }
    return out;
}

static char* dup_cstr(const char* s) {
    size_t n;
    char* p;
    if (s == NULL) {
        s = "";
    }
    n = strlen(s) + 1;
    p = malloc(n);
    if (p == NULL) {
        return NULL;
    }
    memcpy(p, s, n);
    return p;
}

static void free_hook_js(struct hook_js_ctx* hook) {
    if (hook == NULL) {
        return;
    }
    JS_FreeValue(g_ctx, hook->on_enter);
    JS_FreeValue(g_ctx, hook->on_leave);
    JS_FreeValue(g_ctx, hook->impl);
    free(hook);
}

static void remember_art_js(const char* cls, const char* name, const char* sig,
                            struct hook_js_ctx* hook) {
    struct art_js_entry* e = malloc(sizeof(*e));
    if (e == NULL) {
        return;
    }
    e->class_name = dup_cstr(cls);
    e->method_name = dup_cstr(name);
    e->signature = dup_cstr(sig ? sig : "");
    e->hook = hook;
    e->next = g_art_js;
    g_art_js = e;
}

static void forget_art_js(const char* cls, const char* name, const char* sig) {
    struct art_js_entry** pp = &g_art_js;
    const char* s = sig ? sig : "";
    while (*pp != NULL) {
        struct art_js_entry* e = *pp;
        if (strcmp(e->class_name, cls) == 0 && strcmp(e->method_name, name) == 0 &&
            strcmp(e->signature, s) == 0) {
            *pp = e->next;
            free_hook_js(e->hook);
            free(e->class_name);
            free(e->method_name);
            free(e->signature);
            free(e);
            return;
        }
        pp = &e->next;
    }
}

static void art_js_enter(void* ctx, void* env, void* args) {
    struct hook_js_ctx* hook = ctx;
    JSValue arr;
    if (hook == NULL || g_ctx == NULL || !JS_IsFunction(g_ctx, hook->on_enter)) {
        return;
    }
    lock_js();
    if (hook->depth >= 16) {
        unlock_js();
        return;
    }
    hook->depth++;
    enter_js();
    arr = args_to_js((JNIEnv*)env, (jobjectArray)args);
    call_js_hook(hook->on_enter, JS_UNDEFINED, arr);
    JS_FreeValue(g_ctx, arr);
    leave_js();
    hook->depth--;
    unlock_js();
}

static void* art_js_leave(void* ctx, void* env, void* retval) {
    struct hook_js_ctx* hook = ctx;
    JSValue arg;
    JSValue ret;
    jobject out = retval;
    if (hook == NULL || g_ctx == NULL || !JS_IsFunction(g_ctx, hook->on_leave)) {
        return retval;
    }
    lock_js();
    if (hook->depth >= 16) {
        unlock_js();
        return retval;
    }
    hook->depth++;
    enter_js();
    arg = jobject_to_js((JNIEnv*)env, (jobject)retval);
    ret = call_js_hook_ret(hook->on_leave, arg);
    if (!JS_IsUndefined(ret)) {
        out = js_to_jobject((JNIEnv*)env, ret);
    }
    JS_FreeValue(g_ctx, ret);
    JS_FreeValue(g_ctx, arg);
    leave_js();
    hook->depth--;
    unlock_js();
    return out;
}

static void* art_js_impl(void* ctx, void* env, void* args) {
    struct hook_js_ctx* hook = ctx;
    JSValue arr;
    JSValue ret;
    jobject out = NULL;
    if (hook == NULL || g_ctx == NULL || !JS_IsFunction(g_ctx, hook->impl)) {
        return NULL;
    }
    lock_js();
    if (hook->in_impl) {
        unlock_js();
        return art_hook_call_original();
    }
    hook->in_impl = 1;
    enter_js();
    arr = args_to_js((JNIEnv*)env, (jobjectArray)args);
    ret = call_js_hook_ret(hook->impl, arr);
    out = js_to_jobject((JNIEnv*)env, ret);
    JS_FreeValue(g_ctx, ret);
    JS_FreeValue(g_ctx, arr);
    leave_js();
    hook->in_impl = 0;
    unlock_js();
    return out;
}

static JSValue js_java_hook(JSContext* ctx, JSValueConst this_val, int argc,
                            JSValueConst* argv) {
    const char* cls;
    const char* name;
    const char* sig = NULL;
    JSValueConst cb;
    JSValue on_enter;
    JSValue on_leave;
    JSValue impl;
    struct hook_js_ctx* hook;
    int rc;
    (void)this_val;
    if (argc < 3) {
        SP_THROW(ctx, enc_java_hook);
    }
    if (JS_IsString(argv[2]) && argc >= 4) {
        sig = JS_ToCString(ctx, argv[2]);
        cb = argv[3];
    } else {
        cb = argv[2];
    }
    cls = JS_ToCString(ctx, argv[0]);
    name = JS_ToCString(ctx, argv[1]);
    if (cls == NULL || name == NULL) {
        if (cls) {
            JS_FreeCString(ctx, cls);
        }
        if (name) {
            JS_FreeCString(ctx, name);
        }
        if (sig) {
            JS_FreeCString(ctx, sig);
        }
        SP_THROW(ctx, enc_class_method);
    }
    impl = JS_UNDEFINED;
    {
        char enter_name[sizeof(enc_on_enter)];
        char leave_name[sizeof(enc_on_leave)];
        sp_dec(enter_name, enc_on_enter, sizeof(enc_on_enter));
        sp_dec(leave_name, enc_on_leave, sizeof(enc_on_leave));
        on_enter = JS_GetPropertyStr(ctx, cb, enter_name);
        on_leave = JS_GetPropertyStr(ctx, cb, leave_name);
    }
    if (JS_IsFunction(ctx, cb)) {
        impl = JS_DupValue(ctx, cb);
        JS_FreeValue(ctx, on_enter);
        JS_FreeValue(ctx, on_leave);
        on_enter = JS_UNDEFINED;
        on_leave = JS_UNDEFINED;
    } else {
        if (!JS_IsFunction(ctx, on_enter)) {
            JS_FreeValue(ctx, on_enter);
            on_enter = JS_UNDEFINED;
        }
        if (!JS_IsFunction(ctx, on_leave)) {
            JS_FreeValue(ctx, on_leave);
            on_leave = JS_UNDEFINED;
        }
    }
    if (!JS_IsFunction(ctx, on_enter) && !JS_IsFunction(ctx, on_leave) &&
        !JS_IsFunction(ctx, impl)) {
        JS_FreeValue(ctx, on_enter);
        JS_FreeValue(ctx, on_leave);
        JS_FreeValue(ctx, impl);
        JS_FreeCString(ctx, cls);
        JS_FreeCString(ctx, name);
        if (sig) {
            JS_FreeCString(ctx, sig);
        }
        SP_THROW(ctx, enc_on_enter_leave_fn);
    }
    hook = malloc(sizeof(*hook));
    if (hook == NULL) {
        JS_FreeValue(ctx, on_enter);
        JS_FreeValue(ctx, on_leave);
        JS_FreeValue(ctx, impl);
        JS_FreeCString(ctx, cls);
        JS_FreeCString(ctx, name);
        if (sig) {
            JS_FreeCString(ctx, sig);
        }
        return JS_ThrowOutOfMemory(ctx);
    }
    hook->on_enter = on_enter;
    hook->on_leave = on_leave;
    hook->impl = impl;
    hook->in_impl = 0;
    hook->depth = 0;
    rc = art_hook_attach(cls, name, sig,
                         JS_IsFunction(ctx, on_enter) ? art_js_enter : NULL,
                         JS_IsFunction(ctx, on_leave) ? art_js_leave : NULL,
                         JS_IsFunction(ctx, impl) ? art_js_impl : NULL, hook);
    if (rc != 0) {
        free_hook_js(hook);
        JS_FreeCString(ctx, cls);
        JS_FreeCString(ctx, name);
        if (sig) {
            JS_FreeCString(ctx, sig);
        }
        SP_THROW_INTERNAL(ctx, enc_java_hook_failed);
    }
    remember_art_js(cls, name, sig, hook);
    JS_FreeCString(ctx, cls);
    JS_FreeCString(ctx, name);
    if (sig) {
        JS_FreeCString(ctx, sig);
    }
    return JS_UNDEFINED;
}

static JSValue js_java_unhook(JSContext* ctx, JSValueConst this_val, int argc,
                              JSValueConst* argv) {
    const char* cls;
    const char* name;
    const char* sig = NULL;
    int rc;
    (void)this_val;
    if (argc < 2) {
        SP_THROW(ctx, enc_java_unhook);
    }
    cls = JS_ToCString(ctx, argv[0]);
    name = JS_ToCString(ctx, argv[1]);
    if (argc >= 3 && JS_IsString(argv[2])) {
        sig = JS_ToCString(ctx, argv[2]);
    }
    if (cls == NULL || name == NULL) {
        if (cls) {
            JS_FreeCString(ctx, cls);
        }
        if (name) {
            JS_FreeCString(ctx, name);
        }
        if (sig) {
            JS_FreeCString(ctx, sig);
        }
        SP_THROW(ctx, enc_class_method);
    }
    rc = art_hook_detach(cls, name, sig);
    if (rc == 0) {
        forget_art_js(cls, name, sig);
    }
    JS_FreeCString(ctx, cls);
    JS_FreeCString(ctx, name);
    if (sig) {
        JS_FreeCString(ctx, sig);
    }
    if (rc != 0) {
        SP_THROW_INTERNAL(ctx, enc_java_unhook_failed);
    }
    return JS_UNDEFINED;
}

static JSValue js_java_original(JSContext* ctx, JSValueConst this_val, int argc,
                                JSValueConst* argv) {
    JNIEnv* env = art_hook_current_env();
    jobject ret;
    (void)this_val;
    (void)argc;
    (void)argv;
    if (env == NULL) {
        SP_THROW(ctx, enc_java_original);
    }
    ret = art_hook_call_original();
    return jobject_to_js(env, ret);
}

static JSValue js_java_str(JSContext* ctx, JSValueConst this_val, int argc,
                           JSValueConst* argv) {
    uint64_t u = 0;
    char buf[512];
    (void)this_val;
    if (argc < 1) {
        SP_THROW(ctx, enc_java_str);
    }
    if (JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
        return JS_NewString(ctx, "null");
    }
    if (JS_IsString(argv[0])) {
        return JS_DupValue(ctx, argv[0]);
    }
    if (parse_u64(ctx, argv[0], &u) != 0) {
        SP_THROW(ctx, enc_java_str_handle);
    }
    if (art_hook_to_string((void*)(uintptr_t)u, buf, sizeof(buf)) != 0) {
        SP_THROW_INTERNAL(ctx, enc_to_string_failed);
    }
    return JS_NewString(ctx, buf);
}

static JSValue js_java_describe(JSContext* ctx, JSValueConst this_val, int argc,
                                JSValueConst* argv) {
    const char* cls;
    struct art_method_info* list = NULL;
    int n = 0;
    int i;
    int j;
    JSValue arr;
    (void)this_val;
    if (argc < 1) {
        SP_THROW(ctx, enc_java_describe);
    }
    cls = JS_ToCString(ctx, argv[0]);
    if (cls == NULL) {
        return JS_ThrowTypeError(ctx, "class required");
    }
    if (art_hook_list_methods(cls, &list, &n) != 0) {
        JS_FreeCString(ctx, cls);
        SP_THROW_INTERNAL(ctx, enc_java_use_missing);
    }
    JS_FreeCString(ctx, cls);
    arr = JS_NewArray(ctx);
    for (i = 0; i < n; i++) {
        JSValue o = JS_NewObject(ctx);
        JSValue params = JS_NewArray(ctx);
        JS_SetPropertyStr(ctx, o, "name", JS_NewString(ctx, list[i].name));
        JS_SetPropertyStr(ctx, o, "sig", JS_NewString(ctx, list[i].sig));
        JS_SetPropertyStr(ctx, o, "argc", JS_NewInt32(ctx, list[i].argc));
        JS_SetPropertyStr(ctx, o, "isStatic", JS_NewBool(ctx, list[i].is_static));
        JS_SetPropertyStr(ctx, o, "isCtor", JS_NewBool(ctx, list[i].is_ctor));
        for (j = 0; j < list[i].argc; j++) {
            JS_SetPropertyUint32(ctx, params, (uint32_t)j,
                                 JS_NewString(ctx, list[i].params[j]));
        }
        JS_SetPropertyStr(ctx, o, "params", params);
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, o);
    }
    art_hook_list_free(list);
    return arr;
}

static JSValue js_java_invoke(JSContext* ctx, JSValueConst this_val, int argc,
                              JSValueConst* argv) {
    const char* cls;
    const char* name;
    const char* sig;
    JNIEnv* env;
    jobject thiz;
    jobjectArray params;
    int is_static = 0;
    jobject ret;
    JSValue out;
    (void)this_val;
    if (argc < 5) {
        SP_THROW(ctx, enc_java_invoke);
    }
    env = art_hook_jni_env();
    if (env == NULL) {
        return JS_ThrowInternalError(ctx, "no JNIEnv");
    }
    cls = JS_ToCString(ctx, argv[0]);
    name = JS_ToCString(ctx, argv[1]);
    sig = JS_ToCString(ctx, argv[2]);
    if (cls == NULL || name == NULL || sig == NULL) {
        if (cls) {
            JS_FreeCString(ctx, cls);
        }
        if (name) {
            JS_FreeCString(ctx, name);
        }
        if (sig) {
            JS_FreeCString(ctx, sig);
        }
        return JS_ThrowTypeError(ctx, "class, name, sig required");
    }
    if (argc >= 6) {
        is_static = JS_ToBool(ctx, argv[5]);
    }
    thiz = js_to_jobject(env, argv[3]);
    params = js_array_to_jobjects(env, argv[4]);
    ret = art_hook_invoke(cls, name, sig, thiz, params, is_static);
    out = jobject_to_js(env, ret);
    JS_FreeCString(ctx, cls);
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, sig);
    return out;
}

static JSValue js_java_new(JSContext* ctx, JSValueConst this_val, int argc,
                           JSValueConst* argv) {
    const char* cls;
    const char* sig;
    JNIEnv* env;
    jobjectArray params;
    jobject obj;
    (void)this_val;
    if (argc < 3) {
        SP_THROW(ctx, enc_java_new);
    }
    env = art_hook_jni_env();
    if (env == NULL) {
        return JS_ThrowInternalError(ctx, "no JNIEnv");
    }
    cls = JS_ToCString(ctx, argv[0]);
    sig = JS_ToCString(ctx, argv[1]);
    if (cls == NULL || sig == NULL) {
        if (cls) {
            JS_FreeCString(ctx, cls);
        }
        if (sig) {
            JS_FreeCString(ctx, sig);
        }
        return JS_ThrowTypeError(ctx, "class and sig required");
    }
    params = js_array_to_jobjects(env, argv[2]);
    obj = art_hook_new_instance(cls, sig, params);
    JS_FreeCString(ctx, cls);
    JS_FreeCString(ctx, sig);
    if (obj == NULL) {
        return JS_NULL;
    }
    return jobject_to_js(env, obj);
}

static JSValue js_java_class_name(JSContext* ctx, JSValueConst this_val, int argc,
                                  JSValueConst* argv) {
    JNIEnv* env;
    jobject obj;
    char buf[256];
    (void)this_val;
    if (argc < 1) {
        SP_THROW(ctx, enc_java_class_name);
    }
    env = art_hook_jni_env();
    if (env == NULL) {
        return JS_ThrowInternalError(ctx, "no JNIEnv");
    }
    obj = js_to_jobject(env, argv[0]);
    if (obj == NULL || art_hook_class_name(obj, buf, sizeof(buf)) != 0) {
        return JS_NULL;
    }
    return JS_NewString(ctx, buf);
}

static void js_ptr_finalizer(JSRuntime* rt, JSValue val) {
    uint64_t* p = JS_GetOpaque(val, g_ptr_class);
    js_free_rt(rt, p);
}

static JSValue js_ptr_new(JSContext* ctx, uint64_t addr) {
    JSValue obj = JS_NewObjectClass(ctx, (int)g_ptr_class);
    uint64_t* p;
    if (JS_IsException(obj)) {
        return obj;
    }
    p = js_malloc(ctx, sizeof(*p));
    if (p == NULL) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    *p = addr;
    JS_SetOpaque(obj, p);
    return obj;
}

static uint64_t js_ptr_get(JSContext* ctx, JSValueConst this_val, int* ok) {
    uint64_t* p = ptr_opaque(this_val);
    if (p == NULL) {
        {
            char msg[sizeof(enc_not_a_pointer)];
            sp_dec(msg, enc_not_a_pointer, sizeof(enc_not_a_pointer));
            JS_ThrowTypeError(ctx, "%s", msg);
        }
        *ok = 0;
        return 0;
    }
    *ok = 1;
    return *p;
}

static JSValue js_ptr_read_pointer(JSContext* ctx, JSValueConst this_val, int argc,
                                   JSValueConst* argv) {
    int ok = 0;
    uint64_t addr = js_ptr_get(ctx, this_val, &ok);
    uint64_t v = 0;
    (void)argc;
    (void)argv;
    if (!ok) {
        return JS_EXCEPTION;
    }
    if ((uintptr_t)addr == 0) {
        SP_THROW(ctx, enc_null_pointer);
    }
    memcpy(&v, (const void*)(uintptr_t)addr, sizeof(void*));
    return js_ptr_new(ctx, v);
}

static JSValue js_ptr_read_cstring(JSContext* ctx, JSValueConst this_val, int argc,
                                   JSValueConst* argv) {
    int ok = 0;
    uint64_t addr = js_ptr_get(ctx, this_val, &ok);
    const char* s;
    size_t n = 0;
    JSValue out;
    (void)argc;
    (void)argv;
    if (!ok) {
        return JS_EXCEPTION;
    }
    if ((uintptr_t)addr == 0) {
        SP_THROW(ctx, enc_null_pointer);
    }
    s = (const char*)(uintptr_t)addr;
    while (n < 65536 && s[n] != '\0') {
        n++;
    }
    out = JS_NewStringLen(ctx, s, n);
    return out;
}

static int ptr_checked(JSContext* ctx, JSValueConst this_val, uint64_t* addr) {
    int ok = 0;
    *addr = js_ptr_get(ctx, this_val, &ok);
    if (!ok) {
        return -1;
    }
    if (*addr == 0) {
        {
            char msg[sizeof(enc_null_pointer)];
            sp_dec(msg, enc_null_pointer, sizeof(enc_null_pointer));
            JS_ThrowTypeError(ctx, "%s", msg);
        }
        return -1;
    }
    return 0;
}

static int value_addr(JSContext* ctx, JSValueConst v, uint64_t* addr) {
    if (parse_u64(ctx, v, addr) != 0) {
        return -1;
    }
    if (*addr == 0) {
        {
            char msg[sizeof(enc_null_pointer)];
            sp_dec(msg, enc_null_pointer, sizeof(enc_null_pointer));
            JS_ThrowTypeError(ctx, "%s", msg);
        }
        return -1;
    }
    return 0;
}

static JSValue read_int_at(JSContext* ctx, uint64_t addr, int nbytes, int sign, int as_big) {
    uint64_t u = 0;
    int64_t s;
    memcpy(&u, (const void*)(uintptr_t)addr, (size_t)nbytes);
    if (sign && nbytes < 8) {
        unsigned shift = (unsigned)((8 - nbytes) * 8);
        s = (int64_t)(u << shift) >> shift;
    } else {
        s = (int64_t)u;
    }
    if (as_big) {
        return JS_NewBigUint64(ctx, u);
    }
    return JS_NewInt64(ctx, s);
}

/* magic: nbytes | sign<<8 | big<<9 | from_this<<10 */
static JSValue js_read_int(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                           int magic) {
    int nbytes = magic & 0xff;
    int sign = (magic >> 8) & 1;
    int as_big = (magic >> 9) & 1;
    int from_this = (magic >> 10) & 1;
    uint64_t addr;
    if (from_this) {
        (void)argc;
        (void)argv;
        if (ptr_checked(ctx, this_val, &addr) != 0) {
            return JS_EXCEPTION;
        }
    } else {
        if (argc < 1) {
            return JS_ThrowTypeError(ctx, "read");
        }
        if (value_addr(ctx, argv[0], &addr) != 0) {
            return JS_EXCEPTION;
        }
    }
    return read_int_at(ctx, addr, nbytes, sign, as_big);
}

static JSValue js_write_int(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                            int magic) {
    int nbytes = magic & 0xff;
    int from_this = (magic >> 10) & 1;
    uint64_t addr;
    uint64_t x;
    JSValueConst src;
    if (from_this) {
        if (argc < 1) {
            return JS_ThrowTypeError(ctx, "write");
        }
        if (ptr_checked(ctx, this_val, &addr) != 0) {
            return JS_EXCEPTION;
        }
        src = argv[0];
    } else {
        if (argc < 2) {
            return JS_ThrowTypeError(ctx, "write");
        }
        if (value_addr(ctx, argv[0], &addr) != 0) {
            return JS_EXCEPTION;
        }
        src = argv[1];
    }
    if (parse_u64(ctx, src, &x) != 0) {
        return JS_EXCEPTION;
    }
    memcpy((void*)(uintptr_t)addr, &x, (size_t)nbytes);
    if (from_this) {
        return JS_DupValue(ctx, this_val);
    }
    return JS_UNDEFINED;
}

static JSValue js_ptr_read_float(JSContext* ctx, JSValueConst this_val, int argc,
                                 JSValueConst* argv, int magic) {
    uint64_t addr;
    int is_double = magic & 1;
    int from_this = magic & 2;
    JSValueConst src;
    (void)argc;
    if (from_this) {
        if (ptr_checked(ctx, this_val, &addr) != 0) {
            return JS_EXCEPTION;
        }
        src = this_val;
        (void)src;
    } else {
        if (argc < 1 || value_addr(ctx, argv[0], &addr) != 0) {
            return JS_EXCEPTION;
        }
    }
    if (is_double) {
        double d;
        memcpy(&d, (const void*)(uintptr_t)addr, sizeof(d));
        return JS_NewFloat64(ctx, d);
    }
    {
        float f;
        memcpy(&f, (const void*)(uintptr_t)addr, sizeof(f));
        return JS_NewFloat64(ctx, (double)f);
    }
}

static JSValue js_ptr_write_float(JSContext* ctx, JSValueConst this_val, int argc,
                                  JSValueConst* argv, int magic) {
    uint64_t addr;
    int is_double = magic & 1;
    int from_this = magic & 2;
    double d;
    JSValueConst src;
    if (from_this) {
        if (argc < 1) {
            SP_THROW(ctx, enc_write_float);
        }
        if (ptr_checked(ctx, this_val, &addr) != 0) {
            return JS_EXCEPTION;
        }
        src = argv[0];
    } else {
        if (argc < 2) {
            SP_THROW(ctx, enc_write_float);
        }
        if (value_addr(ctx, argv[0], &addr) != 0) {
            return JS_EXCEPTION;
        }
        src = argv[1];
    }
    if (JS_ToFloat64(ctx, &d, src) != 0) {
        return JS_EXCEPTION;
    }
    if (is_double) {
        memcpy((void*)(uintptr_t)addr, &d, sizeof(d));
    } else {
        float f = (float)d;
        memcpy((void*)(uintptr_t)addr, &f, sizeof(f));
    }
    if (from_this) {
        return JS_DupValue(ctx, this_val);
    }
    return JS_UNDEFINED;
}

static JSValue js_read_bytes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                             int from_this) {
    uint64_t addr;
    int64_t len = 0;
    JSValueConst len_val;
    if (from_this) {
        if (argc < 1 || ptr_checked(ctx, this_val, &addr) != 0) {
            return JS_EXCEPTION;
        }
        len_val = argv[0];
    } else {
        if (argc < 2 || value_addr(ctx, argv[0], &addr) != 0) {
            return JS_EXCEPTION;
        }
        len_val = argv[1];
    }
    if (JS_ToInt64Ext(ctx, &len, len_val) != 0 || len < 0 || len > (1 << 24)) {
        SP_THROW(ctx, enc_read_bytes_len);
    }
    return JS_NewArrayBufferCopy(ctx, (const uint8_t*)(uintptr_t)addr, (size_t)len);
}

static JSValue js_ptr_read_bytes(JSContext* ctx, JSValueConst this_val, int argc,
                                 JSValueConst* argv) {
    return js_read_bytes(ctx, this_val, argc, argv, 1);
}

static JSValue js_mem_read_bytes(JSContext* ctx, JSValueConst this_val, int argc,
                                 JSValueConst* argv) {
    (void)this_val;
    return js_read_bytes(ctx, this_val, argc, argv, 0);
}

static JSValue js_write_bytes(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                              int from_this) {
    uint64_t addr;
    size_t len = 0;
    uint8_t* buf;
    JSValueConst src;
    if (from_this) {
        if (argc < 1 || ptr_checked(ctx, this_val, &addr) != 0) {
            return JS_EXCEPTION;
        }
        src = argv[0];
    } else {
        if (argc < 2 || value_addr(ctx, argv[0], &addr) != 0) {
            return JS_EXCEPTION;
        }
        src = argv[1];
    }
    buf = JS_GetArrayBuffer(ctx, &len, src);
    if (buf == NULL) {
        SP_THROW(ctx, enc_write_bytes_buf);
    }
    memcpy((void*)(uintptr_t)addr, buf, len);
    if (from_this) {
        return JS_DupValue(ctx, this_val);
    }
    return JS_UNDEFINED;
}

static JSValue js_ptr_write_bytes(JSContext* ctx, JSValueConst this_val, int argc,
                                  JSValueConst* argv) {
    return js_write_bytes(ctx, this_val, argc, argv, 1);
}

static JSValue js_mem_write_bytes(JSContext* ctx, JSValueConst this_val, int argc,
                                  JSValueConst* argv) {
    (void)this_val;
    return js_write_bytes(ctx, this_val, argc, argv, 0);
}

static JSValue read_utf8(JSContext* ctx, uint64_t addr, int argc, JSValueConst* argv, int argi) {
    const char* s;
    size_t n = 0;
    size_t max = 65536;
    int64_t given = 0;
    if (argc > argi && !JS_IsUndefined(argv[argi]) && !JS_IsNull(argv[argi])) {
        if (JS_ToInt64Ext(ctx, &given, argv[argi]) != 0 || given < 0) {
            SP_THROW(ctx, enc_read_utf8_len);
        }
        max = (size_t)given;
    }
    s = (const char*)(uintptr_t)addr;
    while (n < max && s[n] != '\0') {
        n++;
    }
    return JS_NewStringLen(ctx, s, n);
}

static JSValue js_ptr_read_utf8(JSContext* ctx, JSValueConst this_val, int argc,
                                JSValueConst* argv) {
    uint64_t addr;
    if (ptr_checked(ctx, this_val, &addr) != 0) {
        return JS_EXCEPTION;
    }
    return read_utf8(ctx, addr, argc, argv, 0);
}

static JSValue js_mem_read_utf8(JSContext* ctx, JSValueConst this_val, int argc,
                                JSValueConst* argv) {
    uint64_t addr;
    (void)this_val;
    if (argc < 1 || value_addr(ctx, argv[0], &addr) != 0) {
        return JS_EXCEPTION;
    }
    return read_utf8(ctx, addr, argc, argv, 1);
}

static JSValue write_utf8(JSContext* ctx, JSValueConst this_val, uint64_t addr, JSValueConst src,
                          int from_this) {
    const char* s = JS_ToCString(ctx, src);
    if (s == NULL) {
        return JS_EXCEPTION;
    }
    memcpy((void*)(uintptr_t)addr, s, strlen(s) + 1);
    JS_FreeCString(ctx, s);
    if (from_this) {
        return JS_DupValue(ctx, this_val);
    }
    return JS_UNDEFINED;
}

static JSValue js_ptr_write_utf8(JSContext* ctx, JSValueConst this_val, int argc,
                                 JSValueConst* argv) {
    uint64_t addr;
    if (argc < 1 || ptr_checked(ctx, this_val, &addr) != 0) {
        return JS_EXCEPTION;
    }
    return write_utf8(ctx, this_val, addr, argv[0], 1);
}

static JSValue js_mem_write_utf8(JSContext* ctx, JSValueConst this_val, int argc,
                                 JSValueConst* argv) {
    uint64_t addr;
    (void)this_val;
    if (argc < 2 || value_addr(ctx, argv[0], &addr) != 0) {
        return JS_EXCEPTION;
    }
    return write_utf8(ctx, this_val, addr, argv[1], 0);
}

static JSValue js_ptr_binop(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv,
                            int magic) {
    int ok = 0;
    uint64_t addr = js_ptr_get(ctx, this_val, &ok);
    int64_t rhs = 0;
    uint64_t out;
    if (!ok) {
        return JS_EXCEPTION;
    }
    if (magic == 7) {
        return js_ptr_new(ctx, ~addr);
    }
    if (argc < 1 || JS_ToInt64Ext(ctx, &rhs, argv[0]) != 0) {
        SP_THROW(ctx, enc_pointer_arith);
    }
    switch (magic) {
    case 0:
        out = addr + (uint64_t)rhs;
        break;
    case 1:
        out = addr - (uint64_t)rhs;
        break;
    case 2:
        out = addr & (uint64_t)rhs;
        break;
    case 3:
        out = addr | (uint64_t)rhs;
        break;
    case 4:
        out = addr ^ (uint64_t)rhs;
        break;
    case 5:
        out = addr << (rhs & 63);
        break;
    case 6:
        out = addr >> (rhs & 63);
        break;
    default:
        return JS_EXCEPTION;
    }
    return js_ptr_new(ctx, out);
}

static JSValue js_ptr_equals(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    int ok = 0;
    uint64_t addr = js_ptr_get(ctx, this_val, &ok);
    uint64_t rhs = 0;
    if (!ok) {
        return JS_EXCEPTION;
    }
    if (argc < 1 || parse_u64(ctx, argv[0], &rhs) != 0) {
        return JS_FALSE;
    }
    return JS_NewBool(ctx, addr == rhs);
}

static JSValue js_ptr_compare(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    int ok = 0;
    uint64_t addr = js_ptr_get(ctx, this_val, &ok);
    uint64_t rhs = 0;
    if (!ok) {
        return JS_EXCEPTION;
    }
    if (argc < 1 || parse_u64(ctx, argv[0], &rhs) != 0) {
        SP_THROW(ctx, enc_compare_rhs);
    }
    if (addr < rhs) {
        return JS_NewInt32(ctx, -1);
    }
    if (addr > rhs) {
        return JS_NewInt32(ctx, 1);
    }
    return JS_NewInt32(ctx, 0);
}

static JSValue js_ptr_is_null(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    int ok = 0;
    uint64_t addr = js_ptr_get(ctx, this_val, &ok);
    (void)argc;
    (void)argv;
    if (!ok) {
        return JS_EXCEPTION;
    }
    return JS_NewBool(ctx, addr == 0);
}

static JSValue js_ptr_to_int32(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    int ok = 0;
    uint64_t addr = js_ptr_get(ctx, this_val, &ok);
    (void)argc;
    (void)argv;
    if (!ok) {
        return JS_EXCEPTION;
    }
    return JS_NewInt32(ctx, (int32_t)addr);
}

static JSValue js_ptr_value_of(JSContext* ctx, JSValueConst this_val, int argc,
                               JSValueConst* argv) {
    int ok = 0;
    uint64_t addr = js_ptr_get(ctx, this_val, &ok);
    (void)argc;
    (void)argv;
    if (!ok) {
        return JS_EXCEPTION;
    }
    return JS_NewInt64(ctx, (int64_t)addr);
}

static JSValue js_ptr_to_string(JSContext* ctx, JSValueConst this_val, int argc,
                                JSValueConst* argv) {
    int ok = 0;
    uint64_t addr = js_ptr_get(ctx, this_val, &ok);
    char buf[32];
    (void)argc;
    (void)argv;
    if (!ok) {
        return JS_EXCEPTION;
    }
    if (argc >= 1 && !JS_IsUndefined(argv[0])) {
        int32_t radix = 16;
        if (JS_ToInt32(ctx, &radix, argv[0]) != 0) {
            return JS_EXCEPTION;
        }
        if (radix == 10) {
            snprintf(buf, sizeof(buf), "%llu", (unsigned long long)addr);
            return JS_NewString(ctx, buf);
        }
        if (radix != 16) {
            SP_THROW(ctx, enc_to_string_radix);
        }
    }
    snprintf(buf, sizeof(buf), "0x%llx", (unsigned long long)addr);
    return JS_NewString(ctx, buf);
}

static JSValue js_mem_read_pointer(JSContext* ctx, JSValueConst this_val, int argc,
                                  JSValueConst* argv) {
    uint64_t addr;
    uint64_t v = 0;
    (void)this_val;
    if (argc < 1 || value_addr(ctx, argv[0], &addr) != 0) {
        return JS_EXCEPTION;
    }
    memcpy(&v, (const void*)(uintptr_t)addr, sizeof(v));
    return js_ptr_new(ctx, v);
}

static JSValue js_ptr_write_pointer(JSContext* ctx, JSValueConst this_val, int argc,
                                    JSValueConst* argv) {
    uint64_t addr;
    uint64_t x;
    if (argc < 1 || ptr_checked(ctx, this_val, &addr) != 0 || parse_u64(ctx, argv[0], &x) != 0) {
        return JS_EXCEPTION;
    }
    memcpy((void*)(uintptr_t)addr, &x, sizeof(x));
    return JS_DupValue(ctx, this_val);
}

static JSValue js_mem_write_pointer(JSContext* ctx, JSValueConst this_val, int argc,
                                    JSValueConst* argv) {
    uint64_t addr;
    uint64_t x;
    (void)this_val;
    if (argc < 2 || value_addr(ctx, argv[0], &addr) != 0 || parse_u64(ctx, argv[1], &x) != 0) {
        return JS_EXCEPTION;
    }
    memcpy((void*)(uintptr_t)addr, &x, sizeof(x));
    return JS_UNDEFINED;
}

static JSValue js_mem_alloc(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    int64_t size = 0;
    long ps;
    size_t n;
    void* p;
    (void)this_val;
    if (argc < 1 || JS_ToInt64Ext(ctx, &size, argv[0]) != 0 || size <= 0) {
        SP_THROW(ctx, enc_memory_alloc);
    }
    ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) {
        ps = 4096;
    }
    n = (size_t)size;
    n = (n + (size_t)ps - 1u) & ~((size_t)ps - 1u);
    p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        SP_THROW_INTERNAL(ctx, enc_memory_alloc_failed);
    }
    return js_ptr_new(ctx, (uint64_t)(uintptr_t)p);
}

static JSValue js_global_ptr(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    uint64_t addr = 0;
    (void)this_val;
    if (argc < 1 || parse_u64(ctx, argv[0], &addr) != 0) {
        SP_THROW(ctx, enc_ptr_address);
    }
    return js_ptr_new(ctx, addr);
}

static void* resolve_symbol(const char* lib, const char* name) {
    void* handle;
    if (lib == NULL || lib[0] == '\0') {
        return dlsym(RTLD_DEFAULT, name);
    }
    handle = dlopen(lib, RTLD_NOW | RTLD_NOLOAD);
    if (handle == NULL) {
        handle = dlopen(lib, RTLD_NOW);
    }
    if (handle == NULL) {
        return NULL;
    }
    return dlsym(handle, name);
}

static JSValue js_module_find_export(JSContext* ctx, JSValueConst this_val, int argc,
                                     JSValueConst* argv) {
    const char* lib = NULL;
    const char* name;
    void* sym;
    JSValue out;
    (void)this_val;
    if (argc < 2) {
        SP_THROW(ctx, enc_module_find_export);
    }
    name = JS_ToCString(ctx, argv[1]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    if (!JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0])) {
        lib = JS_ToCString(ctx, argv[0]);
        if (lib == NULL) {
            JS_FreeCString(ctx, name);
            return JS_EXCEPTION;
        }
    }
    sym = resolve_symbol(lib, name);
    if (lib != NULL) {
        JS_FreeCString(ctx, lib);
    }
    JS_FreeCString(ctx, name);
    if (sym == NULL) {
        return JS_NULL;
    }
    out = js_ptr_new(ctx, (uint64_t)(uintptr_t)sym);
    return out;
}

static JSValue js_module_get_export(JSContext* ctx, JSValueConst this_val, int argc,
                                    JSValueConst* argv) {
    JSValue v = js_module_find_export(ctx, this_val, argc, argv);
    if (JS_IsException(v)) {
        return v;
    }
    if (JS_IsNull(v)) {
        SP_THROW_INTERNAL(ctx, enc_export_not_found);
    }
    return v;
}

static JSValue js_process_tid(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewInt32(ctx, (int32_t)gettid());
}

static void js_module_finalizer(JSRuntime* rt, JSValue val) {
    GumModule* module = JS_GetOpaque(val, g_module_class);
    (void)rt;
    if (module != NULL) {
        g_object_unref(module);
    }
}

static GumModule* js_module_get(JSValueConst val) {
    if (g_module_class == 0) {
        return NULL;
    }
    return JS_GetOpaque(val, g_module_class);
}

/* Adopts the caller's reference to @module. */
static JSValue js_module_new(JSContext* ctx, GumModule* module) {
    JSValue obj;
    const GumMemoryRange* range;
    const char* s;
    if (module == NULL) {
        return JS_NULL;
    }
    obj = JS_NewObjectClass(ctx, (int)g_module_class);
    if (JS_IsException(obj)) {
        g_object_unref(module);
        return obj;
    }
    JS_SetOpaque(obj, module);
    s = gum_module_get_name(module);
    JS_SetPropertyStr(ctx, obj, "name", s != NULL ? JS_NewString(ctx, s) : JS_NULL);
    s = gum_module_get_path(module);
    JS_SetPropertyStr(ctx, obj, "path", s != NULL ? JS_NewString(ctx, s) : JS_NULL);
    range = gum_module_get_range(module);
    JS_SetPropertyStr(ctx, obj, "base",
                      js_ptr_new(ctx, range != NULL ? range->base_address : 0));
    JS_SetPropertyStr(ctx, obj, "size",
                      JS_NewInt64(ctx, (int64_t)(range != NULL ? range->size : 0)));
    return obj;
}

struct mod_enum_ctx {
    JSContext* ctx;
    JSValue arr;
    uint32_t idx;
};

static gboolean collect_export(const GumExportDetails* d, gpointer user_data) {
    struct mod_enum_ctx* c = user_data;
    JSValue obj = JS_NewObject(c->ctx);
    JS_SetPropertyStr(c->ctx, obj, "name",
                      d->name != NULL ? JS_NewString(c->ctx, d->name) : JS_NULL);
    JS_SetPropertyStr(c->ctx, obj, "type",
                      JS_NewString(c->ctx, d->type == GUM_EXPORT_FUNCTION ? "function" : "variable"));
    JS_SetPropertyStr(c->ctx, obj, "address", js_ptr_new(c->ctx, d->address));
    JS_SetPropertyUint32(c->ctx, c->arr, c->idx++, obj);
    return TRUE;
}

static gboolean collect_import(const GumImportDetails* d, gpointer user_data) {
    struct mod_enum_ctx* c = user_data;
    JSValue obj = JS_NewObject(c->ctx);
    const char* type = "unknown";
    if (d->type == GUM_IMPORT_FUNCTION) {
        type = "function";
    } else if (d->type == GUM_IMPORT_VARIABLE) {
        type = "variable";
    }
    JS_SetPropertyStr(c->ctx, obj, "type", JS_NewString(c->ctx, type));
    JS_SetPropertyStr(c->ctx, obj, "name",
                      d->name != NULL ? JS_NewString(c->ctx, d->name) : JS_NULL);
    JS_SetPropertyStr(c->ctx, obj, "module",
                      d->module != NULL ? JS_NewString(c->ctx, d->module) : JS_NULL);
    JS_SetPropertyStr(c->ctx, obj, "address", js_ptr_new(c->ctx, d->address));
    JS_SetPropertyStr(c->ctx, obj, "slot", js_ptr_new(c->ctx, d->slot));
    JS_SetPropertyUint32(c->ctx, c->arr, c->idx++, obj);
    return TRUE;
}

static JSValue js_module_enumerate_exports_impl(JSContext* ctx, GumModule* module) {
    struct mod_enum_ctx c;
    c.ctx = ctx;
    c.arr = JS_NewArray(ctx);
    c.idx = 0;
    gum_module_enumerate_exports(module, collect_export, &c);
    return c.arr;
}

static JSValue js_module_enumerate_imports_impl(JSContext* ctx, GumModule* module) {
    struct mod_enum_ctx c;
    c.ctx = ctx;
    c.arr = JS_NewArray(ctx);
    c.idx = 0;
    gum_module_enumerate_imports(module, collect_import, &c);
    return c.arr;
}

static JSValue js_module_inst_find_export(JSContext* ctx, JSValueConst this_val, int argc,
                                          JSValueConst* argv) {
    GumModule* module = js_module_get(this_val);
    const char* name;
    GumAddress addr;
    if (module == NULL) {
        return JS_ThrowTypeError(ctx, "not a module");
    }
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "findExportByName(name)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    addr = gum_module_find_export_by_name(module, name);
    JS_FreeCString(ctx, name);
    if (addr == 0) {
        return JS_NULL;
    }
    return js_ptr_new(ctx, addr);
}

static JSValue js_module_inst_get_export(JSContext* ctx, JSValueConst this_val, int argc,
                                         JSValueConst* argv) {
    JSValue v = js_module_inst_find_export(ctx, this_val, argc, argv);
    if (JS_IsException(v)) {
        return v;
    }
    if (JS_IsNull(v)) {
        return JS_ThrowInternalError(ctx, "export not found");
    }
    return v;
}

static JSValue js_module_inst_enumerate_exports(JSContext* ctx, JSValueConst this_val, int argc,
                                                JSValueConst* argv) {
    GumModule* module = js_module_get(this_val);
    (void)argc;
    (void)argv;
    if (module == NULL) {
        return JS_ThrowTypeError(ctx, "not a module");
    }
    return js_module_enumerate_exports_impl(ctx, module);
}

static JSValue js_module_inst_enumerate_imports(JSContext* ctx, JSValueConst this_val, int argc,
                                                JSValueConst* argv) {
    GumModule* module = js_module_get(this_val);
    (void)argc;
    (void)argv;
    if (module == NULL) {
        return JS_ThrowTypeError(ctx, "not a module");
    }
    return js_module_enumerate_imports_impl(ctx, module);
}

static JSValue js_module_enumerate_modules(JSContext* ctx, JSValueConst this_val, int argc,
                                           JSValueConst* argv) {
    GumModuleMap* map;
    GPtrArray* values;
    JSValue arr;
    guint i;
    (void)this_val;
    (void)argc;
    (void)argv;
    map = gum_module_map_new();
    values = gum_module_map_get_values(map);
    arr = JS_NewArray(ctx);
    for (i = 0; i < values->len; i++) {
        GumModule* module = g_ptr_array_index(values, i);
        JS_SetPropertyUint32(ctx, arr, i, js_module_new(ctx, g_object_ref(module)));
    }
    g_object_unref(map);
    return arr;
}

static JSValue js_module_load(JSContext* ctx, JSValueConst this_val, int argc,
                              JSValueConst* argv) {
    const char* name;
    GumModule* module;
    GError* err = NULL;
    (void)this_val;
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Module.load(name)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    module = gum_module_load(name, &err);
    JS_FreeCString(ctx, name);
    if (err != NULL) {
        g_error_free(err);
    }
    if (module == NULL) {
        return JS_ThrowInternalError(ctx, "Module.load failed");
    }
    return js_module_new(ctx, module);
}

static JSValue js_module_find_base(JSContext* ctx, JSValueConst this_val, int argc,
                                   JSValueConst* argv) {
    const char* name;
    GumModule* module;
    const GumMemoryRange* range;
    JSValue out;
    (void)this_val;
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Module.findBaseAddress(name)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    module = gum_process_find_module_by_name(name);
    JS_FreeCString(ctx, name);
    if (module == NULL) {
        return JS_NULL;
    }
    range = gum_module_get_range(module);
    out = js_ptr_new(ctx, range != NULL ? range->base_address : 0);
    g_object_unref(module);
    return out;
}

static JSValue js_module_enumerate_exports_by_name(JSContext* ctx, JSValueConst this_val, int argc,
                                                   JSValueConst* argv) {
    const char* name;
    GumModule* module;
    JSValue out;
    (void)this_val;
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Module.enumerateExports(name)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    module = gum_process_find_module_by_name(name);
    JS_FreeCString(ctx, name);
    if (module == NULL) {
        return JS_NewArray(ctx);
    }
    out = js_module_enumerate_exports_impl(ctx, module);
    g_object_unref(module);
    return out;
}

static JSValue js_module_enumerate_imports_by_name(JSContext* ctx, JSValueConst this_val, int argc,
                                                   JSValueConst* argv) {
    const char* name;
    GumModule* module;
    JSValue out;
    (void)this_val;
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Module.enumerateImports(name)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    module = gum_process_find_module_by_name(name);
    JS_FreeCString(ctx, name);
    if (module == NULL) {
        return JS_NewArray(ctx);
    }
    out = js_module_enumerate_imports_impl(ctx, module);
    g_object_unref(module);
    return out;
}

static JSValue js_process_get_module(JSContext* ctx, JSValueConst this_val, int argc,
                                     JSValueConst* argv) {
    const char* name;
    GumModule* module;
    (void)this_val;
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Process.getModuleByName(name)");
    }
    name = JS_ToCString(ctx, argv[0]);
    if (name == NULL) {
        return JS_EXCEPTION;
    }
    module = gum_process_find_module_by_name(name);
    JS_FreeCString(ctx, name);
    return js_module_new(ctx, module);
}

static JSValue js_process_find_module_by_addr(JSContext* ctx, JSValueConst this_val, int argc,
                                              JSValueConst* argv) {
    uint64_t addr = 0;
    GumModule* module;
    (void)this_val;
    if (argc < 1 || parse_u64(ctx, argv[0], &addr) != 0) {
        return JS_ThrowTypeError(ctx, "Process.findModuleByAddress(address)");
    }
    module = gum_process_find_module_by_address(addr);
    return js_module_new(ctx, module);
}

struct thread_enum_ctx {
    JSContext* ctx;
    JSValue arr;
    uint32_t idx;
};

static gboolean collect_thread(const GumThreadDetails* d, gpointer user_data) {
    struct thread_enum_ctx* c = user_data;
    JSValue obj = JS_NewObject(c->ctx);
    const char* state = "running";
    switch (d->state) {
        case GUM_THREAD_STOPPED: state = "stopped"; break;
        case GUM_THREAD_WAITING: state = "waiting"; break;
        case GUM_THREAD_UNINTERRUPTIBLE: state = "uninterruptible"; break;
        case GUM_THREAD_HALTED: state = "halted"; break;
        default: state = "running"; break;
    }
    JS_SetPropertyStr(c->ctx, obj, "id", JS_NewInt64(c->ctx, (int64_t)d->id));
    JS_SetPropertyStr(c->ctx, obj, "state", JS_NewString(c->ctx, state));
    if (d->name != NULL) {
        JS_SetPropertyStr(c->ctx, obj, "name", JS_NewString(c->ctx, d->name));
    }
    JS_SetPropertyUint32(c->ctx, c->arr, c->idx++, obj);
    return TRUE;
}

static JSValue js_process_enumerate_threads(JSContext* ctx, JSValueConst this_val, int argc,
                                            JSValueConst* argv) {
    struct thread_enum_ctx c;
    (void)this_val;
    (void)argc;
    (void)argv;
    c.ctx = ctx;
    c.arr = JS_NewArray(ctx);
    c.idx = 0;
    gum_process_enumerate_threads(collect_thread, &c);
    return c.arr;
}

static JSValue js_process_is_debugger(JSContext* ctx, JSValueConst this_val, int argc,
                                      JSValueConst* argv) {
    (void)this_val;
    (void)argc;
    (void)argv;
    return JS_NewBool(ctx, gum_process_is_debugger_attached());
}

static void listener_detach_node(struct listener_node* node) {
    if (node == NULL || node->listener == NULL || g_gum_interceptor == NULL) {
        return;
    }
    gum_interceptor_detach(g_gum_interceptor, (GumInvocationListener*)node->listener);
    g_object_unref(node->listener);
    node->listener = NULL;
}

static JSValue js_listener_detach(JSContext* ctx, JSValueConst this_val, int argc,
                                  JSValueConst* argv) {
    struct listener_node* node;
    (void)argc;
    (void)argv;
    node = JS_GetOpaque(this_val, g_listener_class);
    if (node == NULL) {
        SP_THROW(ctx, enc_not_a_listener);
    }
    listener_detach_node(node);
    return JS_UNDEFINED;
}

static JSValue js_interceptor_detach_all(JSContext* ctx, JSValueConst this_val, int argc,
                                         JSValueConst* argv) {
    struct listener_node* node;
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    for (node = g_listeners; node != NULL; node = node->next) {
        listener_detach_node(node);
    }
    return JS_UNDEFINED;
}

static JSValue js_interceptor_flush(JSContext* ctx, JSValueConst this_val, int argc,
                                    JSValueConst* argv) {
    (void)ctx;
    (void)this_val;
    (void)argc;
    (void)argv;
    if (g_gum_interceptor != NULL) {
        gum_interceptor_flush(g_gum_interceptor);
    }
    return JS_UNDEFINED;
}

struct sp_js_fn {
    const unsigned char* enc;
    size_t enc_len;
    int argc;
    int magic;
    JSCFunction* plain;
    JSCFunctionMagic* magic_fn;
};

static void set_prop_enc(JSContext* ctx, JSValue obj, const unsigned char* enc, size_t n,
                         JSValue v) {
    char name[40];
    sp_dec(name, enc, n);
    JS_SetPropertyStr(ctx, obj, name, v);
}

static void set_cfunc_enc(JSContext* ctx, JSValue obj, const unsigned char* enc, size_t n,
                          JSCFunction* fn, int argc) {
    char name[40];
    sp_dec(name, enc, n);
    JS_SetPropertyStr(ctx, obj, name, JS_NewCFunction(ctx, fn, name, argc));
}

static void bind_js_fns(JSContext* ctx, JSValue obj, const struct sp_js_fn* fns, int count) {
    int i;
    for (i = 0; i < count; i++) {
        char name[40];
        const struct sp_js_fn* f = &fns[i];
        sp_dec(name, f->enc, f->enc_len);
        if (f->magic < 0) {
            JS_SetPropertyStr(ctx, obj, name, JS_NewCFunction(ctx, f->plain, name, f->argc));
        } else {
            JS_SetPropertyStr(ctx, obj, name,
                              JS_NewCFunctionMagic(ctx, f->magic_fn, name, f->argc,
                                                   JS_CFUNC_generic_magic, f->magic));
        }
    }
}

static const struct sp_js_fn ptr_fns[] = {
    {enc_read_pointer, sizeof(enc_read_pointer), 0, -1, js_ptr_read_pointer, NULL},
    {enc_write_pointer, sizeof(enc_write_pointer), 1, -1, js_ptr_write_pointer, NULL},
    {enc_read_cstring, sizeof(enc_read_cstring), 0, -1, js_ptr_read_cstring, NULL},
    {enc_write_byte_array, sizeof(enc_write_byte_array), 1, -1, js_ptr_write_bytes, NULL},
    {enc_read_u8, sizeof(enc_read_u8), 0, 1 | (1 << 10), NULL, js_read_int},
    {enc_read_u16, sizeof(enc_read_u16), 0, 2 | (1 << 10), NULL, js_read_int},
    {enc_read_u32, sizeof(enc_read_u32), 0, 4 | (1 << 10), NULL, js_read_int},
    {enc_read_u64, sizeof(enc_read_u64), 0, 8 | (1 << 9) | (1 << 10), NULL, js_read_int},
    {enc_read_s8, sizeof(enc_read_s8), 0, 1 | (1 << 8) | (1 << 10), NULL, js_read_int},
    {enc_read_s16, sizeof(enc_read_s16), 0, 2 | (1 << 8) | (1 << 10), NULL, js_read_int},
    {enc_read_s32, sizeof(enc_read_s32), 0, 4 | (1 << 8) | (1 << 10), NULL, js_read_int},
    {enc_read_s64, sizeof(enc_read_s64), 0, 8 | (1 << 8) | (1 << 10), NULL, js_read_int},
    {enc_write_u16, sizeof(enc_write_u16), 1, 2 | (1 << 10), NULL, js_write_int},
    {enc_write_u32, sizeof(enc_write_u32), 1, 4 | (1 << 10), NULL, js_write_int},
    {enc_write_u64, sizeof(enc_write_u64), 1, 8 | (1 << 10), NULL, js_write_int},
    {enc_read_float, sizeof(enc_read_float), 0, 2, NULL, js_ptr_read_float},
    {enc_read_double, sizeof(enc_read_double), 0, 3, NULL, js_ptr_read_float},
    {enc_write_float, sizeof(enc_write_float), 1, 2, NULL, js_ptr_write_float},
    {enc_write_double, sizeof(enc_write_double), 1, 3, NULL, js_ptr_write_float},
    {enc_add, sizeof(enc_add), 1, 0, NULL, js_ptr_binop},
    {enc_sub, sizeof(enc_sub), 1, 1, NULL, js_ptr_binop},
    {enc_and, sizeof(enc_and), 1, 2, NULL, js_ptr_binop},
    {enc_or, sizeof(enc_or), 1, 3, NULL, js_ptr_binop},
    {enc_xor, sizeof(enc_xor), 1, 4, NULL, js_ptr_binop},
    {enc_shl, sizeof(enc_shl), 1, 5, NULL, js_ptr_binop},
    {enc_shr, sizeof(enc_shr), 1, 6, NULL, js_ptr_binop},
    {enc_not, sizeof(enc_not), 0, 7, NULL, js_ptr_binop},
    {enc_equals, sizeof(enc_equals), 1, -1, js_ptr_equals, NULL},
    {enc_compare, sizeof(enc_compare), 1, -1, js_ptr_compare, NULL},
    {enc_is_null, sizeof(enc_is_null), 0, -1, js_ptr_is_null, NULL},
    {enc_to_int32, sizeof(enc_to_int32), 0, -1, js_ptr_to_int32, NULL},
    {enc_value_of, sizeof(enc_value_of), 0, -1, js_ptr_value_of, NULL},
    {enc_to_string, sizeof(enc_to_string), 1, -1, js_ptr_to_string, NULL},
};

static const struct sp_js_fn mem_fns[] = {
    {enc_alloc, sizeof(enc_alloc), 1, -1, js_mem_alloc, NULL},
    {enc_read_pointer, sizeof(enc_read_pointer), 1, -1, js_mem_read_pointer, NULL},
    {enc_write_pointer, sizeof(enc_write_pointer), 2, -1, js_mem_write_pointer, NULL},
    {enc_write_byte_array, sizeof(enc_write_byte_array), 2, -1, js_mem_write_bytes, NULL},
    {enc_read_u8, sizeof(enc_read_u8), 1, 1, NULL, js_read_int},
    {enc_read_u16, sizeof(enc_read_u16), 1, 2, NULL, js_read_int},
    {enc_read_u32, sizeof(enc_read_u32), 1, 4, NULL, js_read_int},
    {enc_read_u64, sizeof(enc_read_u64), 1, 8 | (1 << 9), NULL, js_read_int},
    {enc_read_s8, sizeof(enc_read_s8), 1, 1 | (1 << 8), NULL, js_read_int},
    {enc_read_s16, sizeof(enc_read_s16), 1, 2 | (1 << 8), NULL, js_read_int},
    {enc_read_s32, sizeof(enc_read_s32), 1, 4 | (1 << 8), NULL, js_read_int},
    {enc_read_s64, sizeof(enc_read_s64), 1, 8 | (1 << 8), NULL, js_read_int},
    {enc_write_u16, sizeof(enc_write_u16), 2, 2, NULL, js_write_int},
    {enc_write_u32, sizeof(enc_write_u32), 2, 4, NULL, js_write_int},
    {enc_write_u64, sizeof(enc_write_u64), 2, 8, NULL, js_write_int},
    {enc_read_float, sizeof(enc_read_float), 1, 0, NULL, js_ptr_read_float},
    {enc_read_double, sizeof(enc_read_double), 1, 1, NULL, js_ptr_read_float},
    {enc_write_float, sizeof(enc_write_float), 2, 0, NULL, js_ptr_write_float},
    {enc_write_double, sizeof(enc_write_double), 2, 1, NULL, js_ptr_write_float},
};

static void js_cpu_finalizer(JSRuntime* rt, JSValue val) {
    struct spindle_cpu_snap* snap = JS_GetOpaque(val, g_cpu_class);
    js_free_rt(rt, snap);
}

static JSValue js_cpu_new(JSContext* ctx, const struct spindle_cpu_snap* snap) {
    JSValue obj = JS_NewObjectClass(ctx, (int)g_cpu_class);
    struct spindle_cpu_snap* copy;
    int i;
    char name[8];
    if (JS_IsException(obj)) {
        return obj;
    }
    copy = js_malloc(ctx, sizeof(*copy));
    if (copy == NULL) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    *copy = *snap;
    JS_SetOpaque(obj, copy);
    JS_SetPropertyStr(ctx, obj, "pc", js_ptr_new(ctx, snap->pc));
    JS_SetPropertyStr(ctx, obj, "sp", js_ptr_new(ctx, snap->sp));
    JS_SetPropertyStr(ctx, obj, "fp", js_ptr_new(ctx, snap->fp));
    JS_SetPropertyStr(ctx, obj, "lr", js_ptr_new(ctx, snap->lr));
    for (i = 0; i < 29; i++) {
        snprintf(name, sizeof(name), "x%d", i);
        JS_SetPropertyStr(ctx, obj, name, js_ptr_new(ctx, snap->x[i]));
    }
    return obj;
}

static JSValue js_thread_backtrace(JSContext* ctx, JSValueConst this_val, int argc,
                                   JSValueConst* argv) {
    const struct spindle_cpu_snap* snap = NULL;
    int fuzzy = 0;
    uint64_t pcs[SPINDLE_BT_MAX];
    int n;
    int i;
    JSValue arr;
    (void)this_val;
    if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
        snap = JS_GetOpaque(argv[0], g_cpu_class);
        if (snap == NULL) {
            SP_THROW(ctx, enc_thread_backtrace);
        }
    }
    if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
        int32_t kind = 0;
        if (JS_ToInt32(ctx, &kind, argv[1]) != 0) {
            return JS_EXCEPTION;
        }
        if (kind != 1 && kind != 2) {
            SP_THROW(ctx, enc_backtracer_kind);
        }
        fuzzy = kind == 2;
    }
    n = spindle_backtrace(snap, fuzzy, pcs, SPINDLE_BT_MAX);
    arr = JS_NewArray(ctx);
    for (i = 0; i < n; i++) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, js_ptr_new(ctx, pcs[i]));
    }
    return arr;
}

static void js_ptr_init(JSContext* ctx) {
    JSValue proto;
    JSClassDef def;
    char native_pointer[sizeof(enc_native_pointer)];
    char invocation_listener[sizeof(enc_invocation_listener)];
    if (g_ptr_class != 0) {
        return;
    }
    JS_NewClassID(&g_ptr_class);
    memset(&def, 0, sizeof(def));
    sp_dec(native_pointer, enc_native_pointer, sizeof(enc_native_pointer));
    def.class_name = native_pointer;
    def.finalizer = js_ptr_finalizer;
    JS_NewClass(JS_GetRuntime(ctx), g_ptr_class, &def);
    proto = JS_NewObject(ctx);
    bind_js_fns(ctx, proto, ptr_fns, (int)(sizeof(ptr_fns) / sizeof(ptr_fns[0])));
    {
        char read_utf8[sizeof(enc_read_utf8)];
        char write_utf8[sizeof(enc_write_utf8)];
        char read_bytes[sizeof(enc_read_bytes)];
        char write_u8[sizeof(enc_write_u8)];
        sp_dec(read_utf8, enc_read_utf8, sizeof(enc_read_utf8));
        sp_dec(write_utf8, enc_write_utf8, sizeof(enc_write_utf8));
        sp_dec(read_bytes, enc_read_bytes, sizeof(enc_read_bytes));
        sp_dec(write_u8, enc_write_u8, sizeof(enc_write_u8));
        JS_SetPropertyStr(ctx, proto, read_utf8,
                          JS_NewCFunction(ctx, js_ptr_read_utf8, read_utf8, 1));
        JS_SetPropertyStr(ctx, proto, write_utf8,
                          JS_NewCFunction(ctx, js_ptr_write_utf8, write_utf8, 1));
        JS_SetPropertyStr(ctx, proto, read_bytes,
                          JS_NewCFunction(ctx, js_ptr_read_bytes, read_bytes, 1));
        JS_SetPropertyStr(ctx, proto, write_u8,
                          JS_NewCFunctionMagic(ctx, js_write_int, write_u8, 1,
                                               JS_CFUNC_generic_magic, 1 | (1 << 10)));
    }
    JS_SetClassProto(ctx, g_ptr_class, proto);

    JS_NewClassID(&g_listener_class);
    memset(&def, 0, sizeof(def));
    sp_dec(invocation_listener, enc_invocation_listener, sizeof(enc_invocation_listener));
    def.class_name = invocation_listener;
    JS_NewClass(JS_GetRuntime(ctx), g_listener_class, &def);
    proto = JS_NewObject(ctx);
    set_cfunc_enc(ctx, proto, enc_detach, sizeof(enc_detach), js_listener_detach, 0);
    JS_SetClassProto(ctx, g_listener_class, proto);

    JS_NewClassID(&g_cpu_class);
    memset(&def, 0, sizeof(def));
    {
        char cpu[sizeof(enc_cpu_context)];
        sp_dec(cpu, enc_cpu_context, sizeof(enc_cpu_context));
        def.class_name = cpu;
        def.finalizer = js_cpu_finalizer;
        JS_NewClass(JS_GetRuntime(ctx), g_cpu_class, &def);
    }

    {
        char module_class[sizeof(enc_module)];
        char enumerate_exports[sizeof(enc_enumerate_exports)];
        char enumerate_imports[sizeof(enc_enumerate_imports)];
        char find_export[sizeof(enc_find_export)];
        char get_export[sizeof(enc_get_export)];
        JS_NewClassID(&g_module_class);
        memset(&def, 0, sizeof(def));
        sp_dec(module_class, enc_module, sizeof(enc_module));
        def.class_name = module_class;
        def.finalizer = js_module_finalizer;
        JS_NewClass(JS_GetRuntime(ctx), g_module_class, &def);
        proto = JS_NewObject(ctx);
        sp_dec(enumerate_exports, enc_enumerate_exports, sizeof(enc_enumerate_exports));
        sp_dec(enumerate_imports, enc_enumerate_imports, sizeof(enc_enumerate_imports));
        sp_dec(find_export, enc_find_export, sizeof(enc_find_export));
        sp_dec(get_export, enc_get_export, sizeof(enc_get_export));
        JS_SetPropertyStr(ctx, proto, enumerate_exports,
                          JS_NewCFunction(ctx, js_module_inst_enumerate_exports, enumerate_exports, 0));
        JS_SetPropertyStr(ctx, proto, enumerate_imports,
                          JS_NewCFunction(ctx, js_module_inst_enumerate_imports, enumerate_imports, 0));
        JS_SetPropertyStr(ctx, proto, find_export,
                          JS_NewCFunction(ctx, js_module_inst_find_export, find_export, 1));
        JS_SetPropertyStr(ctx, proto, get_export,
                          JS_NewCFunction(ctx, js_module_inst_get_export, get_export, 1));
        JS_SetClassProto(ctx, g_module_class, proto);
    }
}

static int ensure_js_locked(void) {
    JSValue global;
    JSValue rpc;
    JSValue interceptor;
    JSValue console;
    if (g_ctx != NULL) {
        return 0;
    }
    g_rt = JS_NewRuntime();
    if (g_rt == NULL) {
        return -1;
    }
    g_ctx = JS_NewContext(g_rt);
    if (g_ctx == NULL) {
        JS_FreeRuntime(g_rt);
        g_rt = NULL;
        return -1;
    }
    js_ptr_init(g_ctx);
    global = JS_GetGlobalObject(g_ctx);

    rpc = JS_NewObject(g_ctx);
    set_cfunc_enc(g_ctx, rpc, enc_unlock, sizeof(enc_unlock), js_rpc_unlock, 0);
    set_prop_enc(g_ctx, rpc, enc_exports, sizeof(enc_exports), JS_NewObject(g_ctx));
    set_prop_enc(g_ctx, global, enc_rpc, sizeof(enc_rpc), rpc);

    interceptor = JS_NewObject(g_ctx);
    set_cfunc_enc(g_ctx, interceptor, enc_attach, sizeof(enc_attach), js_interceptor_attach, 2);
    {
        char name[sizeof(enc_detach_all)];
        sp_dec(name, enc_detach_all, sizeof(enc_detach_all));
        JS_SetPropertyStr(g_ctx, interceptor, name,
                          JS_NewCFunction(g_ctx, js_interceptor_detach_all, name, 0));
    }
    set_cfunc_enc(g_ctx, interceptor, enc_flush, sizeof(enc_flush), js_interceptor_flush, 0);
    {
        char name[sizeof(enc_interceptor)];
        sp_dec(name, enc_interceptor, sizeof(enc_interceptor));
        JS_SetPropertyStr(g_ctx, global, name, interceptor);
    }
    set_cfunc_enc(g_ctx, global, enc_ptr, sizeof(enc_ptr), js_global_ptr, 1);
    {
        char name[sizeof(enc_native_pointer)];
        sp_dec(name, enc_native_pointer, sizeof(enc_native_pointer));
        JS_SetPropertyStr(g_ctx, global, name,
                          JS_NewCFunction(g_ctx, js_global_ptr, name, 1));
    }
    JS_SetPropertyStr(g_ctx, global, "NULL", js_ptr_new(g_ctx, 0));
    {
        JSValue memory = JS_NewObject(g_ctx);
        bind_js_fns(g_ctx, memory, mem_fns, (int)(sizeof(mem_fns) / sizeof(mem_fns[0])));
        {
            char read_utf8[sizeof(enc_read_utf8)];
            char write_utf8[sizeof(enc_write_utf8)];
            char read_bytes[sizeof(enc_read_bytes)];
            char write_u8[sizeof(enc_write_u8)];
            sp_dec(read_utf8, enc_read_utf8, sizeof(enc_read_utf8));
            sp_dec(write_utf8, enc_write_utf8, sizeof(enc_write_utf8));
            sp_dec(read_bytes, enc_read_bytes, sizeof(enc_read_bytes));
            sp_dec(write_u8, enc_write_u8, sizeof(enc_write_u8));
            JS_SetPropertyStr(g_ctx, memory, read_utf8,
                              JS_NewCFunction(g_ctx, js_mem_read_utf8, read_utf8, 2));
            JS_SetPropertyStr(g_ctx, memory, write_utf8,
                              JS_NewCFunction(g_ctx, js_mem_write_utf8, write_utf8, 2));
            JS_SetPropertyStr(g_ctx, memory, read_bytes,
                              JS_NewCFunction(g_ctx, js_mem_read_bytes, read_bytes, 2));
            JS_SetPropertyStr(g_ctx, memory, write_u8,
                              JS_NewCFunctionMagic(g_ctx, js_write_int, write_u8, 2,
                                                   JS_CFUNC_generic_magic, 1));
        }
        {
            char name[sizeof(enc_memory)];
            sp_dec(name, enc_memory, sizeof(enc_memory));
            JS_SetPropertyStr(g_ctx, global, name, memory);
        }
    }
    {
        JSValue module = JS_NewObject(g_ctx);
        char find_name[sizeof(enc_find_export)];
        char get_name[sizeof(enc_get_export)];
        char module_name[sizeof(enc_module)];
        char enumerate_modules[sizeof(enc_enumerate_modules)];
        char enumerate_exports[sizeof(enc_enumerate_exports)];
        char enumerate_imports[sizeof(enc_enumerate_imports)];
        char load_name[sizeof(enc_load)];
        char find_base[sizeof(enc_find_base_address)];
        sp_dec(find_name, enc_find_export, sizeof(enc_find_export));
        sp_dec(get_name, enc_get_export, sizeof(enc_get_export));
        sp_dec(module_name, enc_module, sizeof(enc_module));
        sp_dec(enumerate_modules, enc_enumerate_modules, sizeof(enc_enumerate_modules));
        sp_dec(enumerate_exports, enc_enumerate_exports, sizeof(enc_enumerate_exports));
        sp_dec(enumerate_imports, enc_enumerate_imports, sizeof(enc_enumerate_imports));
        sp_dec(load_name, enc_load, sizeof(enc_load));
        sp_dec(find_base, enc_find_base_address, sizeof(enc_find_base_address));
        JS_SetPropertyStr(g_ctx, module, find_name,
                          JS_NewCFunction(g_ctx, js_module_find_export, find_name, 2));
        JS_SetPropertyStr(g_ctx, module, get_name,
                          JS_NewCFunction(g_ctx, js_module_get_export, get_name, 2));
        JS_SetPropertyStr(g_ctx, module, enumerate_modules,
                          JS_NewCFunction(g_ctx, js_module_enumerate_modules, enumerate_modules, 0));
        JS_SetPropertyStr(g_ctx, module, enumerate_exports,
                          JS_NewCFunction(g_ctx, js_module_enumerate_exports_by_name, enumerate_exports, 1));
        JS_SetPropertyStr(g_ctx, module, enumerate_imports,
                          JS_NewCFunction(g_ctx, js_module_enumerate_imports_by_name, enumerate_imports, 1));
        JS_SetPropertyStr(g_ctx, module, load_name,
                          JS_NewCFunction(g_ctx, js_module_load, load_name, 1));
        JS_SetPropertyStr(g_ctx, module, find_base,
                          JS_NewCFunction(g_ctx, js_module_find_base, find_base, 1));
        JS_SetPropertyStr(g_ctx, global, module_name, module);
    }
    {
        JSValue process = JS_NewObject(g_ctx);
        char name[sizeof(enc_process)];
        char tid_name[sizeof(enc_get_tid)];
        char arch_name[sizeof(enc_arch)];
        char platform_name[sizeof(enc_platform)];
        char page_size[sizeof(enc_page_size)];
        char pointer_size[sizeof(enc_pointer_size)];
        char code_signing[sizeof(enc_code_signing_policy)];
        char enumerate_modules[sizeof(enc_enumerate_modules)];
        char get_module[sizeof(enc_get_module_by_name)];
        char find_module[sizeof(enc_find_module_by_name)];
        char find_module_addr[sizeof(enc_find_module_by_address)];
        char enumerate_threads[sizeof(enc_enumerate_threads)];
        char is_debugger[sizeof(enc_is_debugger_attached)];
        sp_dec(name, enc_process, sizeof(enc_process));
        sp_dec(tid_name, enc_get_tid, sizeof(enc_get_tid));
        sp_dec(arch_name, enc_arch, sizeof(enc_arch));
        sp_dec(platform_name, enc_platform, sizeof(enc_platform));
        sp_dec(page_size, enc_page_size, sizeof(enc_page_size));
        sp_dec(pointer_size, enc_pointer_size, sizeof(enc_pointer_size));
        sp_dec(code_signing, enc_code_signing_policy, sizeof(enc_code_signing_policy));
        sp_dec(enumerate_modules, enc_enumerate_modules, sizeof(enc_enumerate_modules));
        sp_dec(get_module, enc_get_module_by_name, sizeof(enc_get_module_by_name));
        sp_dec(find_module, enc_find_module_by_name, sizeof(enc_find_module_by_name));
        sp_dec(find_module_addr, enc_find_module_by_address, sizeof(enc_find_module_by_address));
        sp_dec(enumerate_threads, enc_enumerate_threads, sizeof(enc_enumerate_threads));
        sp_dec(is_debugger, enc_is_debugger_attached, sizeof(enc_is_debugger_attached));
        JS_SetPropertyStr(g_ctx, process, "id", JS_NewInt64(g_ctx, (int64_t)gum_process_get_id()));
        JS_SetPropertyStr(g_ctx, process, arch_name, JS_NewString(g_ctx, "arm64"));
        JS_SetPropertyStr(g_ctx, process, platform_name, JS_NewString(g_ctx, "linux"));
        JS_SetPropertyStr(g_ctx, process, page_size,
                          JS_NewInt32(g_ctx, (int32_t)gum_query_page_size()));
        JS_SetPropertyStr(g_ctx, process, pointer_size, JS_NewInt32(g_ctx, 8));
        JS_SetPropertyStr(g_ctx, process, code_signing,
                          JS_NewString(g_ctx, gum_code_signing_policy_to_string(
                              gum_process_get_code_signing_policy())));
        JS_SetPropertyStr(g_ctx, process, tid_name,
                          JS_NewCFunction(g_ctx, js_process_tid, tid_name, 0));
        JS_SetPropertyStr(g_ctx, process, enumerate_modules,
                          JS_NewCFunction(g_ctx, js_module_enumerate_modules, enumerate_modules, 0));
        JS_SetPropertyStr(g_ctx, process, get_module,
                          JS_NewCFunction(g_ctx, js_process_get_module, get_module, 1));
        JS_SetPropertyStr(g_ctx, process, find_module,
                          JS_NewCFunction(g_ctx, js_process_get_module, find_module, 1));
        JS_SetPropertyStr(g_ctx, process, find_module_addr,
                          JS_NewCFunction(g_ctx, js_process_find_module_by_addr, find_module_addr, 1));
        JS_SetPropertyStr(g_ctx, process, enumerate_threads,
                          JS_NewCFunction(g_ctx, js_process_enumerate_threads, enumerate_threads, 0));
        JS_SetPropertyStr(g_ctx, process, is_debugger,
                          JS_NewCFunction(g_ctx, js_process_is_debugger, is_debugger, 0));
        JS_SetPropertyStr(g_ctx, global, name, process);
    }
    {
        JSValue thread = JS_NewObject(g_ctx);
        JSValue backtracer = JS_NewObject(g_ctx);
        char accurate[sizeof(enc_accurate)];
        char fuzzy[sizeof(enc_fuzzy)];
        char backtracer_name[sizeof(enc_backtracer)];
        {
            char bt_name[sizeof(enc_backtrace)];
            sp_dec(bt_name, enc_backtrace, sizeof(enc_backtrace));
            JS_SetPropertyStr(g_ctx, thread, bt_name,
                              JS_NewCFunction(g_ctx, js_thread_backtrace, bt_name, 2));
        }
        set_prop_enc(g_ctx, global, enc_thread, sizeof(enc_thread), thread);
        sp_dec(accurate, enc_accurate, sizeof(enc_accurate));
        sp_dec(fuzzy, enc_fuzzy, sizeof(enc_fuzzy));
        sp_dec(backtracer_name, enc_backtracer, sizeof(enc_backtracer));
        JS_SetPropertyStr(g_ctx, backtracer, accurate, JS_NewInt32(g_ctx, 1));
        JS_SetPropertyStr(g_ctx, backtracer, fuzzy, JS_NewInt32(g_ctx, 2));
        JS_SetPropertyStr(g_ctx, global, backtracer_name, backtracer);
    }
    spindle_stalker_bind(g_ctx);

    {
        JSValue java = JS_NewObject(g_ctx);
        set_cfunc_enc(g_ctx, java, enc_hook, sizeof(enc_hook), js_java_hook, 4);
        set_cfunc_enc(g_ctx, java, enc_unhook, sizeof(enc_unhook), js_java_unhook, 3);
        set_cfunc_enc(g_ctx, java, enc_original, sizeof(enc_original), js_java_original, 0);
        JS_SetPropertyStr(g_ctx, java, "str",
                          JS_NewCFunction(g_ctx, js_java_str, "str", 1));
        {
            char describe[sizeof(enc_describe)];
            char invoke[sizeof(enc_invoke)];
            char class_name[sizeof(enc_class_name)];
            sp_dec(describe, enc_describe, sizeof(enc_describe));
            sp_dec(invoke, enc_invoke, sizeof(enc_invoke));
            sp_dec(class_name, enc_class_name, sizeof(enc_class_name));
            JS_SetPropertyStr(g_ctx, java, describe,
                              JS_NewCFunction(g_ctx, js_java_describe, describe, 1));
            JS_SetPropertyStr(g_ctx, java, invoke,
                              JS_NewCFunction(g_ctx, js_java_invoke, invoke, 6));
            JS_SetPropertyStr(g_ctx, java, class_name,
                              JS_NewCFunction(g_ctx, js_java_class_name, class_name, 1));
        }
        set_cfunc_enc(g_ctx, java, enc_new, sizeof(enc_new), js_java_new, 3);
        {
            char name[sizeof(enc_java)];
            sp_dec(name, enc_java, sizeof(enc_java));
            JS_SetPropertyStr(g_ctx, global, name, java);
        }
        {
            char js[sizeof(k_java_use_js)];
            char name[sizeof(enc_java_use_name)];
            JSValue loaded;
            sp_dec(js, k_java_use_js, sizeof(k_java_use_js));
            sp_dec(name, enc_java_use_name, sizeof(enc_java_use_name));
            loaded = JS_Eval(g_ctx, js, strlen(js), name, JS_EVAL_TYPE_GLOBAL);
            if (JS_IsException(loaded)) {
                log_js_exception();
            } else {
                JS_FreeValue(g_ctx, loaded);
            }
        }
    }

    console = JS_NewObject(g_ctx);
    JS_SetPropertyStr(g_ctx, console, "log",
                      JS_NewCFunction(g_ctx, js_console_log, "log", 1));
    JS_SetPropertyStr(g_ctx, console, "warn",
                      JS_NewCFunction(g_ctx, js_console_log, "warn", 1));
    JS_SetPropertyStr(g_ctx, console, "error",
                      JS_NewCFunction(g_ctx, js_console_log, "error", 1));
    JS_SetPropertyStr(g_ctx, console, "debug",
                      JS_NewCFunction(g_ctx, js_console_log, "debug", 1));
    JS_SetPropertyStr(g_ctx, global, "console", console);

    JS_FreeValue(g_ctx, global);
    return 0;
}

static char* json_wrap(int ok, const char* msg) {
    static const char k_ok[] = "{\"ok\":true,\"v\":\"";
    static const char k_err[] = "{\"ok\":false,\"e\":\"";
    const char* prefix = ok ? k_ok : k_err;
    size_t prefix_len = ok ? sizeof(k_ok) - 1 : sizeof(k_err) - 1;
    size_t i;
    size_t n;
    size_t cap;
    char* out;
    char* p;
    char rpc_error[sizeof(enc_rpc_error)];
    if (msg == NULL) {
        if (ok) {
            msg = "null";
        } else {
            sp_dec(rpc_error, enc_rpc_error, sizeof(enc_rpc_error));
            msg = rpc_error;
        }
    }
    n = strlen(msg);
    cap = prefix_len + n * 6 + 3;
    out = malloc(cap);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, prefix, prefix_len);
    p = out + prefix_len;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)msg[i];
        if (c == '"' || c == '\\') {
            *p++ = '\\';
            *p++ = (char)c;
        } else if (c == '\n') {
            *p++ = '\\';
            *p++ = 'n';
        } else if (c == '\r') {
            *p++ = '\\';
            *p++ = 'r';
        } else if (c < 0x20) {
            static const char hex[] = "0123456789abcdef";
            *p++ = '\\';
            *p++ = 'u';
            *p++ = '0';
            *p++ = '0';
            *p++ = hex[c >> 4];
            *p++ = hex[c & 0xf];
        } else {
            *p++ = (char)c;
        }
    }
    *p++ = '"';
    *p++ = '}';
    *p = '\0';
    return out;
}

static char* take_exception_error(void) {
    JSValue ex = JS_GetException(g_ctx);
    const char* s = JS_ToCString(g_ctx, ex);
    char rpc_exception[sizeof(enc_rpc_exception)];
    char* out;
    sp_dec(rpc_exception, enc_rpc_exception, sizeof(enc_rpc_exception));
    out = json_wrap(0, s ? s : rpc_exception);
    if (s) {
        log_cstr(s);
        JS_FreeCString(g_ctx, s);
    }
    JS_FreeValue(g_ctx, ex);
    return out;
}

static char* dup_js_string(JSValue v) {
    const char* s = JS_ToCString(g_ctx, v);
    char* out = NULL;
    if (s != NULL) {
        out = strdup(s);
        JS_FreeCString(g_ctx, s);
    }
    return out;
}

int spindle_rpc_call(const char* json, int json_len, char** out) {
    JSValue req = JS_UNDEFINED;
    JSValue global = JS_UNDEFINED;
    JSValue rpc = JS_UNDEFINED;
    JSValue exports = JS_UNDEFINED;
    JSValue method = JS_UNDEFINED;
    JSValue fn = JS_UNDEFINED;
    JSValue args = JS_UNDEFINED;
    JSValue ret = JS_UNDEFINED;
    JSValue box = JS_UNDEFINED;
    JSValue text = JS_UNDEFINED;
    JSValue* argv = NULL;
    const char* name = NULL;
    int argc = 0;
    int i;
    int32_t alen = 0;

    if (out == NULL) {
        return -1;
    }
    *out = NULL;
    if (json == NULL || json_len < 2) {
        {
            char msg[sizeof(enc_empty_rpc)];
            sp_dec(msg, enc_empty_rpc, sizeof(enc_empty_rpc));
            *out = json_wrap(0, msg);
        }
        return -1;
    }
    lock_js();
    if (ensure_js_locked() != 0) {
        unlock_js();
        *out = json_wrap(0, "js init failed");
        return -1;
    }
    enter_js();
    {
        char rpc_file[sizeof(enc_rpc_file)];
        char* json_buf = malloc((size_t)json_len + 1);
        sp_dec(rpc_file, enc_rpc_file, sizeof(enc_rpc_file));
        if (json_buf == NULL) {
            *out = json_wrap(0, "out of memory");
            goto done;
        }
        memcpy(json_buf, json, (size_t)json_len);
        json_buf[json_len] = '\0';
        req = JS_ParseJSON(g_ctx, json_buf, (size_t)json_len, rpc_file);
        free(json_buf);
    }
    if (JS_IsException(req)) {
        *out = take_exception_error();
        goto done;
    }
    global = JS_GetGlobalObject(g_ctx);
    {
        char rpc_name[sizeof(enc_rpc)];
        char exports_name[sizeof(enc_exports)];
        sp_dec(rpc_name, enc_rpc, sizeof(enc_rpc));
        sp_dec(exports_name, enc_exports, sizeof(enc_exports));
        rpc = JS_GetPropertyStr(g_ctx, global, rpc_name);
        exports = JS_GetPropertyStr(g_ctx, rpc, exports_name);
    }
    method = JS_GetPropertyStr(g_ctx, req, "m");
    name = JS_ToCString(g_ctx, method);
    if (name == NULL) {
        JSValue ex = JS_GetException(g_ctx);
        JS_FreeValue(g_ctx, ex);
    }
    if (name == NULL || name[0] == '\0' || !JS_IsObject(exports)) {
        {
            char msg[sizeof(enc_rpc_exports_missing)];
            sp_dec(msg, enc_rpc_exports_missing, sizeof(enc_rpc_exports_missing));
            *out = json_wrap(0, msg);
        }
        goto done;
    }
    fn = JS_GetPropertyStr(g_ctx, exports, name);
    if (!JS_IsFunction(g_ctx, fn)) {
        {
            char msg[sizeof(enc_no_such_export)];
            sp_dec(msg, enc_no_such_export, sizeof(enc_no_such_export));
            *out = json_wrap(0, msg);
        }
        goto done;
    }
    args = JS_GetPropertyStr(g_ctx, req, "a");
    if (!(JS_IsUndefined(args) || JS_IsNull(args))) {
        JSValue lv;
        if (JS_IsArray(g_ctx, args) < 0) {
            *out = take_exception_error();
            goto done;
        }
        if (JS_IsArray(g_ctx, args) != 1) {
            *out = json_wrap(0, "args must be an array");
            goto done;
        }
        lv = JS_GetPropertyStr(g_ctx, args, "length");
        if (JS_ToInt32(g_ctx, &alen, lv) != 0 || alen < 0 || alen > 64) {
            JS_FreeValue(g_ctx, lv);
            *out = json_wrap(0, "bad args");
            goto done;
        }
        JS_FreeValue(g_ctx, lv);
        argc = (int)alen;
    }
    if (argc > 0) {
        argv = malloc(sizeof(*argv) * (size_t)argc);
        if (argv == NULL) {
            *out = json_wrap(0, "out of memory");
            goto done;
        }
        for (i = 0; i < argc; i++) {
            argv[i] = JS_GetPropertyUint32(g_ctx, args, (uint32_t)i);
        }
    }
    ret = JS_Call(g_ctx, fn, exports, argc, argv);
    if (JS_IsException(ret)) {
        *out = take_exception_error();
        ret = JS_UNDEFINED;
        goto done;
    }
    if (JS_IsUndefined(ret)) {
        *out = strdup("{\"ok\":true,\"v\":null}");
        goto done;
    }
    box = JS_NewObject(g_ctx);
    JS_SetPropertyStr(g_ctx, box, "ok", JS_NewBool(g_ctx, 1));
    JS_SetPropertyStr(g_ctx, box, "v", ret);
    ret = JS_UNDEFINED;
    text = JS_JSONStringify(g_ctx, box, JS_UNDEFINED, JS_UNDEFINED);
    if (JS_IsException(text)) {
        JSValue ex = JS_GetException(g_ctx);
        JSValue v = JS_GetPropertyStr(g_ctx, box, "v");
        char* fallback = dup_js_string(v);
        JS_FreeValue(g_ctx, ex);
        JS_FreeValue(g_ctx, v);
        *out = json_wrap(1, fallback ? fallback : "null");
        free(fallback);
        text = JS_UNDEFINED;
        goto done;
    }
    *out = dup_js_string(text);

done:
    if (name != NULL) {
        JS_FreeCString(g_ctx, name);
    }
    for (i = 0; i < argc; i++) {
        if (argv != NULL) {
            JS_FreeValue(g_ctx, argv[i]);
        }
    }
    free(argv);
    JS_FreeValue(g_ctx, req);
    JS_FreeValue(g_ctx, global);
    JS_FreeValue(g_ctx, rpc);
    JS_FreeValue(g_ctx, exports);
    JS_FreeValue(g_ctx, method);
    JS_FreeValue(g_ctx, fn);
    JS_FreeValue(g_ctx, args);
    JS_FreeValue(g_ctx, ret);
    JS_FreeValue(g_ctx, box);
    JS_FreeValue(g_ctx, text);
    if (JS_HasException(g_ctx)) {
        JSValue ex = JS_GetException(g_ctx);
        JS_FreeValue(g_ctx, ex);
    }
    leave_js();
    unlock_js();
    if (*out == NULL) {
        {
        char msg[sizeof(enc_rpc_failed)];
        sp_dec(msg, enc_rpc_failed, sizeof(enc_rpc_failed));
        *out = json_wrap(0, msg);
    }
        return -1;
    }
    return 0;
}

int spindle_run_js(const char* source, int source_len) {
    JSValue ret;
    int rc = 0;
    char* buf;
    if (source == NULL || source_len <= 0) {
        return -1;
    }
    /* JS_Eval requires input[input_len] == '\0'. A JNI array has no such byte. */
    buf = malloc((size_t)source_len + 1);
    if (buf == NULL) {
        return -1;
    }
    memcpy(buf, source, (size_t)source_len);
    buf[source_len] = '\0';
    lock_js();
    if (ensure_js_locked() != 0) {
        unlock_js();
        free(buf);
        return -1;
    }
    enter_js();
    ret = JS_Eval(g_ctx, buf, (size_t)source_len, "<tcp>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(ret)) {
        JSValue ex = JS_GetException(g_ctx);
        const char* s = JS_ToCString(g_ctx, ex);
        log_cstr(s ? s : "eval exception");
        if (s) {
            JS_FreeCString(g_ctx, s);
        }
        JS_FreeValue(g_ctx, ex);
        rc = -1;
    } else {
        JS_FreeValue(g_ctx, ret);
    }
    leave_js();
    unlock_js();
    free(buf);
    return rc;
}
