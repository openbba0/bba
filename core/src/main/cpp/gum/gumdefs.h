/*
 * Copyright (C) 2008-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2024 Yannis Juglaret <yjuglaret@mozilla.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUMDEFS_H__
#define __GUMDEFS_H__

#include <glib.h>
#include <gum/gumenumtypes.h>

#  define GUM_API

G_BEGIN_DECLS

#define GUM_ERROR gum_error_quark ()

typedef enum {
  GUM_ERROR_FAILED,
  GUM_ERROR_NOT_FOUND,
  GUM_ERROR_EXISTS,
  GUM_ERROR_PERMISSION_DENIED,
  GUM_ERROR_INVALID_ARGUMENT,
  GUM_ERROR_NOT_SUPPORTED,
  GUM_ERROR_INVALID_DATA,
} GumError;

typedef guint64 GumAddress;
#define GUM_ADDRESS(a) ((GumAddress) (guintptr) (a))
#define GUM_TYPE_ADDRESS (gum_address_get_type ())
typedef guint GumOS;
typedef guint GumCallingConvention;
typedef guint GumAbiType;
typedef guint GumCpuFeatures;
typedef guint GumInstructionEncoding;
typedef guint GumArgType;
typedef struct _GumArgument GumArgument;
typedef guint GumBranchHint;
typedef struct _GumArm64CpuContext GumArm64CpuContext;
typedef union _GumArm64VectorReg GumArm64VectorReg;
typedef guint GumRelocationScenario;

# define GUM_NATIVE_CPU GUM_CPU_ARM64
# define GUM_DEFAULT_CS_ARCH CS_ARCH_ARM64
# define gum_cs_arch_register_native cs_arch_register_arm64
/**
 * GUM_DEFAULT_CS_MODE: (skip)
 */
# define GUM_DEFAULT_CS_MODE GUM_DEFAULT_CS_ENDIAN
typedef GumArm64CpuContext GumCpuContext;
# define GUM_DEFAULT_CS_ENDIAN CS_MODE_LITTLE_ENDIAN
# define GUM_NATIVE_ABI            GUM_ABI_UNIX
# define GUM_NATIVE_ABI_IS_WINDOWS 0
# define GUM_NATIVE_ABI_IS_UNIX    1

enum _GumOS
{
  GUM_OS_WINDOWS,
  GUM_OS_MACOS,
  GUM_OS_LINUX,
  GUM_OS_IOS,
  GUM_OS_WATCHOS,
  GUM_OS_TVOS,
  GUM_OS_ANDROID,
  GUM_OS_FREEBSD,
  GUM_OS_QNX
};

enum _GumCallingConvention
{
  GUM_CALL_CAPI,
  GUM_CALL_SYSAPI
};

enum _GumAbiType
{
  GUM_ABI_UNIX,
  GUM_ABI_WINDOWS
};

typedef enum {
  GUM_CPU_INVALID,
  GUM_CPU_IA32,
  GUM_CPU_AMD64,
  GUM_CPU_ARM,
  GUM_CPU_ARM64,
  GUM_CPU_MIPS
} GumCpuType;

enum _GumCpuFeatures
{
  GUM_CPU_AVX2            = 1 << 0,
  GUM_CPU_CET_SS          = 1 << 1,
  GUM_CPU_THUMB_INTERWORK = 1 << 2,
  GUM_CPU_VFP2            = 1 << 3,
  GUM_CPU_VFP3            = 1 << 4,
  GUM_CPU_VFPD32          = 1 << 5,
  GUM_CPU_PTRAUTH         = 1 << 6,
};

typedef enum {
  GUM_MEMORY_ACCESS_OPEN,
  GUM_MEMORY_ACCESS_EXCLUSIVE,
} GumMemoryAccess;

enum _GumInstructionEncoding
{
  GUM_INSTRUCTION_DEFAULT,
  GUM_INSTRUCTION_SPECIAL
};

enum _GumArgType
{
  GUM_ARG_ADDRESS,
  GUM_ARG_REGISTER
};

struct _GumArgument
{
  GumArgType type;

  union
  {
    GumAddress address;
    gint reg;
  } value;
};

enum _GumBranchHint
{
  GUM_NO_HINT,
  GUM_LIKELY,
  GUM_UNLIKELY
};

union _GumArm64VectorReg
{
  guint8 q[16];
  gdouble d;
  gfloat s;
  guint16 h;
  guint8 b;
};

struct _GumArm64CpuContext
{
  guint64 pc;
  guint64 sp;
  guint64 nzcv;

  guint64 x[29];
  guint64 fp;
  guint64 lr;

  GumArm64VectorReg v[32];
};

enum _GumRelocationScenario
{
  GUM_SCENARIO_OFFLINE,
  GUM_SCENARIO_ONLINE
};

#  define GUM_CPU_CONTEXT_XAX(c) ((c)->rax)
#  define GUM_CPU_CONTEXT_XCX(c) ((c)->rcx)
#  define GUM_CPU_CONTEXT_XDX(c) ((c)->rdx)
#  define GUM_CPU_CONTEXT_XBX(c) ((c)->rbx)
#  define GUM_CPU_CONTEXT_XSP(c) ((c)->rsp)
#  define GUM_CPU_CONTEXT_XBP(c) ((c)->rbp)
#  define GUM_CPU_CONTEXT_XSI(c) ((c)->rsi)
#  define GUM_CPU_CONTEXT_XDI(c) ((c)->rdi)
#  define GUM_CPU_CONTEXT_XIP(c) ((c)->rip)
#  define GUM_CPU_CONTEXT_OFFSET_XAX (G_STRUCT_OFFSET (GumCpuContext, rax))
#  define GUM_CPU_CONTEXT_OFFSET_XCX (G_STRUCT_OFFSET (GumCpuContext, rcx))
#  define GUM_CPU_CONTEXT_OFFSET_XDX (G_STRUCT_OFFSET (GumCpuContext, rdx))
#  define GUM_CPU_CONTEXT_OFFSET_XBX (G_STRUCT_OFFSET (GumCpuContext, rbx))
#  define GUM_CPU_CONTEXT_OFFSET_XSP (G_STRUCT_OFFSET (GumCpuContext, rsp))
#  define GUM_CPU_CONTEXT_OFFSET_XBP (G_STRUCT_OFFSET (GumCpuContext, rbp))
#  define GUM_CPU_CONTEXT_OFFSET_XSI (G_STRUCT_OFFSET (GumCpuContext, rsi))
#  define GUM_CPU_CONTEXT_OFFSET_XDI (G_STRUCT_OFFSET (GumCpuContext, rdi))
#  define GUM_CPU_CONTEXT_OFFSET_XIP (G_STRUCT_OFFSET (GumCpuContext, rip))

#define GUM_MAX_PATH                 260
#define GUM_MAX_TYPE_NAME             16
#define GUM_MAX_SYMBOL_NAME         2048

#define GUM_MAX_THREADS              768
#define GUM_MAX_CALL_DEPTH            32
#define GUM_MAX_BACKTRACE_DEPTH       16
#define GUM_MAX_WORST_CASE_INFO_SIZE 128

#define GUM_MAX_LISTENERS_PER_FUNCTION 2
#define GUM_MAX_LISTENER_DATA       1024

#define GUM_MAX_THREAD_RANGES 2

# if G_BYTE_ORDER == G_LITTLE_ENDIAN
#  define GUM_CPU_MODE CS_MODE_LITTLE_ENDIAN
# else
#  define GUM_CPU_MODE CS_MODE_BIG_ENDIAN
# endif
# define GUM_X86_THUNK_REG_ARG0 GUM_X86_XDI
# define GUM_X86_THUNK_REG_ARG1 GUM_X86_XSI
#define GUM_RED_ZONE_SIZE 128

# define GUM_CDECL
# define GUM_STDCALL
# define GUM_FASTCALL

# define GUM_NOINLINE __attribute__ ((noinline))

#define GUM_ALIGN_POINTER(t, p, b) \
    ((t) GSIZE_TO_POINTER (((GPOINTER_TO_SIZE (p) + ((gsize) (b - 1))) & \
        ~((gsize) (b - 1)))))
#define GUM_ALIGN_SIZE(s, b) \
    ((((gsize) s) + ((gsize) (b - 1))) & ~((gsize) (b - 1)))

#define GUM_FUNCPTR_TO_POINTER(f) (GSIZE_TO_POINTER (f))
#define GUM_POINTER_TO_FUNCPTR(t, p) ((t) GPOINTER_TO_SIZE (p))

#define GUM_INT2_MASK  0x00000003U
#define GUM_INT3_MASK  0x00000007U
#define GUM_INT4_MASK  0x0000000fU
#define GUM_INT5_MASK  0x0000001fU
#define GUM_INT6_MASK  0x0000003fU
#define GUM_INT8_MASK  0x000000ffU
#define GUM_INT10_MASK 0x000003ffU
#define GUM_INT11_MASK 0x000007ffU
#define GUM_INT12_MASK 0x00000fffU
#define GUM_INT14_MASK 0x00003fffU
#define GUM_INT16_MASK 0x0000ffffU
#define GUM_INT18_MASK 0x0003ffffU
#define GUM_INT19_MASK 0x0007ffffU
#define GUM_INT24_MASK 0x00ffffffU
#define GUM_INT26_MASK 0x03ffffffU
#define GUM_INT28_MASK 0x0fffffffU
#define GUM_INT32_MASK 0xffffffffU

#define GUM_IS_WITHIN_UINT7_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (0) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (127))
#define GUM_IS_WITHIN_UINT8_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (0) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (255))
#define GUM_IS_WITHIN_INT8_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-128) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (127))
#define GUM_IS_WITHIN_INT11_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-1024) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (1023))
#define GUM_IS_WITHIN_INT14_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-8192) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (8191))
#define GUM_IS_WITHIN_INT16_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-32768) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (32767))
#define GUM_IS_WITHIN_INT18_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-131072) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (131071))
#define GUM_IS_WITHIN_INT19_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-262144) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (262143))
#define GUM_IS_WITHIN_INT20_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-524288) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (524287))
#define GUM_IS_WITHIN_INT21_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-1048576) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (1048575))
#define GUM_IS_WITHIN_INT24_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-8388608) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (8388607))
#define GUM_IS_WITHIN_INT26_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-33554432) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (33554431))
#define GUM_IS_WITHIN_INT28_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-134217728) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (134217727))
#define GUM_IS_WITHIN_INT32_RANGE(i) \
    (((gint64) (i)) >= (gint64) G_MININT32 && \
     ((gint64) (i)) <= (gint64) G_MAXINT32)

#ifdef G_NORETURN
# define GUM_NORETURN G_NORETURN
#else
# define GUM_NORETURN
#endif

GUM_API GQuark gum_error_quark (void);

GUM_API GUM_NORETURN void gum_panic (const gchar * format, ...)
    G_ANALYZER_NORETURN;

GUM_API GumCpuFeatures gum_query_cpu_features (void);

GUM_API gpointer gum_cpu_context_get_nth_argument (GumCpuContext * self,
    guint n);
GUM_API void gum_cpu_context_replace_nth_argument (GumCpuContext * self,
    guint n, gpointer value);
GUM_API gpointer gum_cpu_context_get_return_value (GumCpuContext * self);
GUM_API void gum_cpu_context_replace_return_value (GumCpuContext * self,
    gpointer value);

GUM_API GType gum_address_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
