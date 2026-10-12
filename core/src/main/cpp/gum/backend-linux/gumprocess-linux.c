/*
 * Copyright (C) 2010-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023-2024 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2023 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2023 Grant Douglas <me@hexplo.it>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumprocess-priv.h"
#include "spindle_str.h"

#include "gum-init.h"
#include "gummodule-elf.h"
#include "gum/gumandroid.h"
#include "gum/gumlinux.h"
#include "gumlinux-priv.h"
#include "gummodulemap.h"
#include "valgrind.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
# include <pthread.h>
# include <link.h>
#ifdef HAVE_ASM_PRCTL_H
# include <asm/prctl.h>
#endif
#include <sys/prctl.h>
#include <sys/ptrace.h>
#ifdef HAVE_ASM_PTRACE_H
# include <asm/ptrace.h>
#endif
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_USER_H
# include <sys/user.h>
#endif

#define GUM_PAGE_START(value, page_size) \
    (GUM_ADDRESS (value) & ~GUM_ADDRESS (page_size - 1))

#ifndef O_CLOEXEC
# define O_CLOEXEC 0x80000
#endif

#define GUM_PSR_THUMB 0x20

typedef struct user_pt_regs GumGPRegs;
typedef struct _GumArm64DebugRegs GumDebugRegs;
typedef guint GumMipsWatchStyle;
typedef struct _GumMips32WatchRegs GumMips32WatchRegs;
typedef struct _GumMips64WatchRegs GumMips64WatchRegs;
typedef union _GumRegs GumRegs;
#ifndef PTRACE_GETREGS
# define PTRACE_GETREGS 12
#endif
#ifndef PTRACE_SETREGS
# define PTRACE_SETREGS 13
#endif
#ifndef PTRACE_GETHBPREGS
# define PTRACE_GETHBPREGS 29
#endif
#ifndef PTRACE_SETHBPREGS
# define PTRACE_SETHBPREGS 30
#endif
#ifndef PTRACE_GET_WATCH_REGS
# define PTRACE_GET_WATCH_REGS 0xd0
#endif
#ifndef PTRACE_SET_WATCH_REGS
# define PTRACE_SET_WATCH_REGS 0xd1
#endif
#ifndef PTRACE_GETREGSET
# define PTRACE_GETREGSET 0x4204
#endif
#ifndef PTRACE_SETREGSET
# define PTRACE_SETREGSET 0x4205
#endif
#ifndef PR_SET_PTRACER
# define PR_SET_PTRACER 0x59616d61
#endif
#ifndef NT_PRSTATUS
# define NT_PRSTATUS 1
#endif

#define GUM_TEMP_FAILURE_RETRY(expression) \
    ({ \
      gssize __result; \
      \
      do __result = (gssize) (expression); \
      while (__result == -EINTR); \
      \
      __result; \
    })

typedef struct _GumProgramModules GumProgramModules;
typedef guint GumProgramRuntimeLinker;
typedef struct _GumProgramRanges GumProgramRanges;
typedef ElfW(auxv_t) * (* GumReadAuxvFunc) (void);

typedef struct _GumModifyThreadContext GumModifyThreadContext;
typedef void (* GumLinuxModifyThreadFunc) (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);
typedef struct _GumLinuxModifyThreadContext GumLinuxModifyThreadContext;
typedef guint GumLinuxRegsType;
typedef guint8 GumModifyThreadAck;

typedef struct _GumEnumerateModulesContext GumEnumerateModulesContext;
typedef struct _GumResolveModuleNameContext GumResolveModuleNameContext;

typedef gint (* GumFoundDlPhdrFunc) (struct dl_phdr_info * info,
    gsize size, gpointer data);
typedef void (* GumDlIteratePhdrImpl) (GumFoundDlPhdrFunc func, gpointer data);

typedef struct _GumSetHardwareBreakpointContext GumSetHardwareBreakpointContext;
typedef struct _GumSetHardwareWatchpointContext GumSetHardwareWatchpointContext;

typedef struct _GumUserDesc GumUserDesc;
typedef struct _GumTcbHead GumTcbHead;

typedef gint (* GumCloneFunc) (gpointer arg);

struct _GumProgramModules
{
  GumModule * program;
  GumModule * interpreter;
  GumModule * vdso;
  GumProgramRuntimeLinker rtld;
};

enum _GumProgramRuntimeLinker
{
  GUM_PROGRAM_RTLD_NONE,
  GUM_PROGRAM_RTLD_SHARED,
};

struct _GumProgramRanges
{
  GumMemoryRange program;
  GumMemoryRange interpreter;
  GumMemoryRange vdso;
};

struct _GumModifyThreadContext
{
  GumModifyThreadFunc func;
  gpointer user_data;
};

enum _GumLinuxRegsType
{
  GUM_REGS_GENERAL_PURPOSE,
  GUM_REGS_DEBUG_BREAK,
  GUM_REGS_DEBUG_WATCH,
};

struct _GumX86DebugRegs
{
  gsize dr0;
  gsize dr1;
  gsize dr2;
  gsize dr3;
  gsize dr6;
  gsize dr7;
};

struct _GumArmDebugRegs
{
  guint32 cr[16];
  guint32 vr[16];
};

struct _GumArm64DebugRegs
{
  guint64 cr[16];
  guint64 vr[16];
};

enum _GumMipsWatchStyle
{
  GUM_MIPS_WATCH_MIPS32,
  GUM_MIPS_WATCH_MIPS64,
};

struct _GumMips32WatchRegs
{
  guint32 watch_lo[8];
  guint16 watch_hi[8];
  guint16 watch_masks[8];
  guint32 num_valid;
} __attribute__ ((aligned (8)));

struct _GumMips64WatchRegs
{
  guint64 watch_lo[8];
  guint16 watch_hi[8];
  guint16 watch_masks[8];
  guint32 num_valid;
} __attribute__ ((aligned (8)));

struct _GumMipsDebugRegs
{
  GumMipsWatchStyle style;
  union
  {
    GumMips32WatchRegs mips32;
    GumMips64WatchRegs mips64;
  };
};

union _GumRegs
{
  GumGPRegs gp;
  GumDebugRegs debug;
};

enum _GumModifyThreadAck
{
  GUM_ACK_READY = 1,
  GUM_ACK_READ_REGISTERS,
  GUM_ACK_MODIFIED_REGISTERS,
  GUM_ACK_WROTE_REGISTERS,
  GUM_ACK_FAILED_TO_ATTACH,
  GUM_ACK_FAILED_TO_WAIT,
  GUM_ACK_FAILED_TO_STOP,
  GUM_ACK_FAILED_TO_READ,
  GUM_ACK_FAILED_TO_WRITE,
  GUM_ACK_FAILED_TO_DETACH
};

struct _GumLinuxModifyThreadContext
{
  GumThreadId thread_id;
  GumLinuxRegsType regs_type;
  GumLinuxModifyThreadFunc func;
  gpointer user_data;

  gint fd[2];
  GumRegs regs_data;
};

struct _GumEnumerateModulesContext
{
  GumFoundModuleFunc func;
  gpointer user_data;

  GHashTable * named_ranges;
};

struct _GumEmitExecutableModuleContext
{
  const gchar * executable_path;
  GumFoundModuleFunc func;
  gpointer user_data;

  gboolean carry_on;
};

struct _GumResolveModuleNameContext
{
  const gchar * name;
  GumAddress known_address;
  gchar * path;
  GumAddress base;
};

struct _GumSetHardwareBreakpointContext
{
  guint breakpoint_id;
  GumAddress address;
};

struct _GumSetHardwareWatchpointContext
{
  guint watchpoint_id;
  GumAddress address;
  gsize size;
  GumWatchConditions conditions;
};

struct _GumUserDesc
{
  guint entry_number;
  guint base_addr;
  guint limit;
  guint seg_32bit : 1;
  guint contents : 2;
  guint read_exec_only : 1;
  guint limit_in_pages : 1;
  guint seg_not_present : 1;
  guint useable : 1;
};

struct _GumTcbHead
{
  gpointer dtv;
  gpointer priv;
};

static void gum_deinit_program_modules (void);
static gboolean gum_query_program_ranges (GumReadAuxvFunc read_auxv,
    GumProgramRanges * ranges);
static ElfW(auxv_t) * gum_read_auxv_from_proc (void);
static ElfW(auxv_t) * gum_read_auxv_from_stack (void);
static gboolean gum_query_main_thread_stack_range (GumMemoryRange * range);
static void gum_compute_elf_range_from_ehdr (const ElfW(Ehdr) * ehdr,
    GumMemoryRange * range);
static void gum_compute_elf_range_from_phdrs (const ElfW(Phdr) * phdrs,
    ElfW(Half) phdr_size, ElfW(Half) phdr_count, GumAddress base_address,
    GumMemoryRange * range);

static gboolean gum_try_resolve_dynamic_symbol (const gchar * name,
    Dl_info * info);

static void gum_do_modify_thread (GumThreadId thread_id, GumRegs * regs,
    gpointer user_data);
static gboolean gum_linux_modify_thread (GumThreadId thread_id,
    GumLinuxRegsType regs_type, GumLinuxModifyThreadFunc func,
    gpointer user_data, GError ** error);
static gpointer gum_linux_handle_modify_thread_comms (gpointer data);
static gint gum_linux_do_modify_thread (gpointer data);
static gboolean gum_await_ack (gint fd, GumModifyThreadAck expected_ack);
static void gum_put_ack (gint fd, GumModifyThreadAck ack);

static void gum_store_cpu_context (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);

static GumModule * gum_try_init_libc_module (void);
static void gum_deinit_libc_module (void);
static const Dl_info * gum_process_get_libc_info (void);
static const Dl_info * gum_try_init_libc_info (void);
static void gum_process_enumerate_modules_by_using_libc (
    GumDlIteratePhdrImpl iterate_phdr, GumFoundModuleFunc func,
    gpointer user_data);
static gint gum_emit_module_from_phdr (struct dl_phdr_info * info, gsize size,
    gpointer user_data);
static gpointer gum_create_module_handle (GumNativeModule * module,
    gpointer user_data);

static void gum_linux_named_range_free (GumLinuxNamedRange * range);
static gboolean gum_try_translate_vdso_name (gchar * name);
static void gum_do_set_hardware_breakpoint (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);
static void gum_do_unset_hardware_breakpoint (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);
static void gum_do_set_hardware_watchpoint (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);
static void gum_do_unset_hardware_watchpoint (GumThreadId thread_id,
    GumRegs * regs, gpointer user_data);

static void gum_proc_maps_iter_init_for_path (GumProcMapsIter * iter,
    const gchar * path);

static void gum_acquire_dumpability (void);
static void gum_release_dumpability (void);

static gchar * gum_thread_read_name (GumThreadId thread_id);
static gboolean gum_thread_read_state (GumThreadId tid, GumThreadState * state);
static GumThreadState gum_thread_state_from_proc_status_character (gchar c);
static GumPageProtection gum_page_protection_from_proc_perms_string (
    const gchar * perms);

static gssize gum_get_regs (pid_t pid, guint type, gpointer data, gsize * size);
static gssize gum_set_regs (pid_t pid, guint type, gconstpointer data,
    gsize size);

static void gum_parse_gp_regs (const GumGPRegs * regs, GumCpuContext * ctx);
static void gum_unparse_gp_regs (const GumCpuContext * ctx, GumGPRegs * regs);

static gssize gum_libc_clone (GumCloneFunc child_func, gpointer child_stack,
    gint flags, gpointer arg, pid_t * parent_tidptr, GumUserDesc * tls,
    pid_t * child_tidptr);
static gssize gum_libc_read (gint fd, gpointer buf, gsize count);
static gssize gum_libc_write (gint fd, gconstpointer buf, gsize count);
static pid_t gum_libc_waitpid (pid_t pid, int * status, int options);
static gssize gum_libc_ptrace (gsize request, pid_t pid, gpointer address,
    gpointer data);

#define gum_libc_syscall_3(n, a, b, c) gum_libc_syscall_4 (n, a, b, c, 0)
static gssize gum_libc_syscall_4 (gsize n, gsize a, gsize b, gsize c, gsize d);

static GumProgramModules gum_program_modules;
static GumModule * gum_libc_module;
static Dl_info gum_libc_info;

static gboolean gum_is_regset_supported = TRUE;

G_LOCK_DEFINE_STATIC (gum_dumpable);
static gint gum_dumpable_refcount = 0;
static gint gum_dumpable_previous = 0;

static const GumProgramModules *
gum_query_program_modules (void)
{
  static gsize modules_value = 0;

  if (g_once_init_enter (&modules_value))
  {
    static GumProgramRanges ranges;
    gboolean got_kern, got_user;
    GumProgramRanges kern, user;
    GumProcMapsIter iter;
    gchar * path;
    const gchar * line;

    got_kern = gum_query_program_ranges (gum_read_auxv_from_proc, &kern);
    got_user = gum_query_program_ranges (gum_read_auxv_from_stack, &user);
    if (got_kern && got_user &&
        user.program.base_address != kern.program.base_address)
    {
      ranges = user;
      ranges.interpreter = kern.program;
    }
    else if (got_kern)
      ranges = kern;
    else
      ranges = user;

    gum_program_modules.rtld = (ranges.interpreter.base_address == 0)
        ? GUM_PROGRAM_RTLD_NONE
        : GUM_PROGRAM_RTLD_SHARED;

    gum_proc_maps_iter_init_for_self (&iter);
    path = g_malloc (PATH_MAX);

    while (gum_proc_maps_iter_next (&iter, &line))
    {
      GumAddress start;
      GumModule ** m;
      const GumMemoryRange * r;

      sscanf (line, "%" G_GINT64_MODIFIER "x-", &start);

      if (start == ranges.program.base_address)
      {
        m = &gum_program_modules.program;
        r = &ranges.program;
      }
      else if (start == ranges.interpreter.base_address)
      {
        m = &gum_program_modules.interpreter;
        r = &ranges.interpreter;
      }
      else
        continue;

      sscanf (line, "%*x-%*x %*c%*c%*c%*c %*x %*s %*d %[^\n]", path);

      *m = GUM_MODULE (_gum_native_module_make_handleless (path, r));
    }

    g_free (path);
    gum_proc_maps_iter_destroy (&iter);

    if (ranges.vdso.base_address != 0)
    {
      /* FIXME: Parse soname instead of hardcoding: */
      gum_program_modules.vdso = GUM_MODULE (
          _gum_native_module_make_handleless ("linux-vdso.so.1", &ranges.vdso));
    }

    _gum_register_destructor (gum_deinit_program_modules);

    g_once_init_leave (&modules_value, GPOINTER_TO_SIZE (&gum_program_modules));
  }

  return GSIZE_TO_POINTER (modules_value);
}

static void
gum_deinit_program_modules (void)
{
  GumProgramModules * m = &gum_program_modules;

  g_object_unref (m->program);
  g_object_unref (m->interpreter);
  g_object_unref (m->vdso);
}

static gboolean
gum_query_program_ranges (GumReadAuxvFunc read_auxv,
                          GumProgramRanges * ranges)
{
  gboolean success = FALSE;
  ElfW(auxv_t) * auxv;
  const ElfW(Phdr) * phdrs;
  ElfW(Half) phdr_size, phdr_count;
  const ElfW(Ehdr) * interpreter, * vdso;
  ElfW(auxv_t) * entry;

  bzero (ranges, sizeof (GumProgramRanges));

  auxv = read_auxv ();
  if (auxv == NULL)
    goto beach;

  phdrs = NULL;
  phdr_size = 0;
  phdr_count = 0;
  interpreter = NULL;
  vdso = NULL;
  for (entry = auxv; entry->a_type != AT_NULL; entry++)
  {
    switch (entry->a_type)
    {
      case AT_PHDR:
        phdrs = (ElfW(Phdr) *) entry->a_un.a_val;
        break;
      case AT_PHENT:
        phdr_size = entry->a_un.a_val;
        break;
      case AT_PHNUM:
        phdr_count = entry->a_un.a_val;
        break;
      case AT_BASE:
        interpreter = (const ElfW(Ehdr) *) entry->a_un.a_val;
        break;
      case AT_SYSINFO_EHDR:
        vdso = (const ElfW(Ehdr) *) entry->a_un.a_val;
        break;
    }
  }
  if (phdrs == NULL || phdr_size == 0 || phdr_count == 0)
    goto beach;

  gum_compute_elf_range_from_phdrs (phdrs, phdr_size, phdr_count, 0,
      &ranges->program);
  gum_compute_elf_range_from_ehdr (interpreter, &ranges->interpreter);
  gum_compute_elf_range_from_ehdr (vdso, &ranges->vdso);

  success = TRUE;

beach:
  g_free (auxv);

  return success;
}

static ElfW(auxv_t) *
gum_read_auxv_from_proc (void)
{
  ElfW(auxv_t) * auxv = NULL;

  gum_acquire_dumpability ();

  g_file_get_contents ("/proc/self/auxv", (gchar **) &auxv, NULL, NULL);

  gum_release_dumpability ();

  return auxv;
}

static ElfW(auxv_t) *
gum_read_auxv_from_stack (void)
{
  GumMemoryRange stack;
  gpointer stack_start, stack_end;
  ElfW(auxv_t) needle;
  const ElfW(auxv_t) * match, * last_match;
  gsize offset;
  const ElfW(auxv_t) * cursor, * auxv_start, * auxv_end;
  gsize page_size;

  if (!gum_query_main_thread_stack_range (&stack))
    return NULL;
  stack_start = GSIZE_TO_POINTER (stack.base_address);
  stack_end = stack_start + stack.size;

  needle.a_type = AT_PHENT;
  needle.a_un.a_val = sizeof (ElfW(Phdr));

  match = NULL;
  last_match = NULL;
  offset = 0;
  while (offset != stack.size)
  {
    match = memmem (GSIZE_TO_POINTER (stack.base_address) + offset,
        stack.size - offset, &needle, sizeof (needle));
    if (match == NULL)
      break;

    last_match = match;
    offset = (GUM_ADDRESS (match) - stack.base_address) + 1;
  }
  if (last_match == NULL)
    return NULL;

  auxv_start = NULL;
  page_size = gum_query_page_size ();
  for (cursor = last_match - 1;
      (gpointer) cursor >= stack_start;
      cursor--)
  {
    gboolean probably_an_invalid_type = cursor->a_type >= page_size;
    if (probably_an_invalid_type)
    {
      auxv_start = cursor + 1;
      break;
    }
  }

  auxv_end = NULL;
  for (cursor = last_match + 1;
      (gpointer) cursor <= stack_end - sizeof (ElfW(auxv_t));
      cursor++)
  {
    if (cursor->a_type == AT_NULL)
    {
      auxv_end = cursor + 1;
      break;
    }
  }
  if (auxv_end == NULL)
    return NULL;

  return g_memdup (auxv_start, (guint8 *) auxv_end - (guint8 *) auxv_start);
}

static gboolean
gum_query_main_thread_stack_range (GumMemoryRange * range)
{
  GumProcMapsIter iter;
  GumAddress stack_bottom, stack_top;
  const gchar * line;

  gum_proc_maps_iter_init_for_self (&iter);

  stack_bottom = 0;
  stack_top = 0;

  while (gum_proc_maps_iter_next (&iter, &line))
  {
    if (g_str_has_suffix (line, " [stack]"))
    {
      sscanf (line,
          "%" G_GINT64_MODIFIER "x-%" G_GINT64_MODIFIER "x ",
          &stack_bottom,
          &stack_top);
      break;
    }
  }

  range->base_address = stack_bottom;
  range->size = stack_top - stack_bottom;

  gum_proc_maps_iter_destroy (&iter);

  return range->size != 0;
}

static void
gum_compute_elf_range_from_ehdr (const ElfW(Ehdr) * ehdr,
                                 GumMemoryRange * range)
{
  if (ehdr == NULL)
  {
    range->base_address = 0;
    range->size = 0;
    return;
  }

  gum_compute_elf_range_from_phdrs ((gconstpointer) ehdr + ehdr->e_phoff,
      ehdr->e_phentsize, ehdr->e_phnum, GUM_ADDRESS (ehdr), range);
}

static void
gum_compute_elf_range_from_phdrs (const ElfW(Phdr) * phdrs,
                                  ElfW(Half) phdr_size,
                                  ElfW(Half) phdr_count,
                                  GumAddress base_address,
                                  GumMemoryRange * range)
{
  GumAddress lowest, highest;
  gsize page_size;
  ElfW(Half) i;
  const ElfW(Phdr) * phdr;

  range->base_address = 0;

  lowest = ~0;
  highest = 0;
  page_size = gum_query_page_size ();

  for (i = 0, phdr = phdrs;
      i != phdr_count;
      i++, phdr = (gconstpointer) phdr + phdr_size)
  {
    if (phdr->p_type == PT_PHDR)
      range->base_address = GPOINTER_TO_SIZE (phdrs) - phdr->p_offset;

    if (phdr->p_type == PT_LOAD && phdr->p_offset == 0)
    {
      if (range->base_address == 0)
        range->base_address = phdr->p_vaddr;
    }

    if (phdr->p_type == PT_LOAD)
    {
      lowest = MIN (GUM_PAGE_START (phdr->p_vaddr, page_size), lowest);
      highest = MAX (phdr->p_vaddr + phdr->p_memsz, highest);
    }
  }

  if (range->base_address == 0)
  {
    range->base_address = (base_address != 0)
        ? base_address
        : GUM_PAGE_START (phdrs, page_size);
  }

  range->size = highest - lowest;
}

static gboolean
gum_try_resolve_dynamic_symbol (const gchar * name,
                                Dl_info * info)
{
  gpointer address;

  address = dlsym (RTLD_NEXT, name);
  if (address == NULL)
    address = dlsym (RTLD_DEFAULT, name);
  if (address == NULL)
    return FALSE;

  return dladdr (address, info) != 0;
}

gboolean
gum_process_is_debugger_attached (void)
{
  gboolean result;
  gchar * status, * p;

  status = NULL;
  g_file_get_contents ("/proc/self/status", &status, NULL, NULL);

  p = strstr (status, "TracerPid:");
  g_assert (p != NULL);

  result = atoi (p + 10) != 0;

  g_free (status);

  return result;
}

GumProcessId
gum_process_get_id (void)
{
  return getpid ();
}

GumThreadId
gum_process_get_current_thread_id (void)
{
  return syscall (__NR_gettid);
}

gboolean
gum_process_has_thread (GumThreadId thread_id)
{
  gchar path[16 + 20 + 1];
  sprintf (path, "/proc/self/task/%" G_GSIZE_MODIFIER "u", thread_id);

  return g_file_test (path, G_FILE_TEST_EXISTS);
}

gboolean
gum_process_modify_thread (GumThreadId thread_id,
                           GumModifyThreadFunc func,
                           gpointer user_data,
                           GumModifyThreadFlags flags)
{
  GumModifyThreadContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  return gum_linux_modify_thread (thread_id, GUM_REGS_GENERAL_PURPOSE,
      gum_do_modify_thread, &ctx, NULL);
}

static void
gum_do_modify_thread (GumThreadId thread_id,
                      GumRegs * regs,
                      gpointer user_data)
{
  GumGPRegs * gpr = &regs->gp;
  GumModifyThreadContext * ctx = user_data;
  GumCpuContext cpu_context;

  gum_parse_gp_regs (gpr, &cpu_context);

  ctx->func (thread_id, &cpu_context, ctx->user_data);

  gum_unparse_gp_regs (&cpu_context, gpr);
}

static gboolean
gum_linux_modify_thread (GumThreadId thread_id,
                         GumLinuxRegsType regs_type,
                         GumLinuxModifyThreadFunc func,
                         gpointer user_data,
                         GError ** error)
{
  gboolean success = FALSE;
  GumLinuxModifyThreadContext ctx;
  gssize child;
  gpointer stack = NULL;
  gpointer tls = NULL;
  GumUserDesc * desc;

  ctx.thread_id = thread_id;
  ctx.regs_type = regs_type;
  ctx.func = func;
  ctx.user_data = user_data;

  ctx.fd[0] = -1;
  ctx.fd[1] = -1;

  memset (&ctx.regs_data, 0, sizeof (ctx.regs_data));

  if (socketpair (AF_UNIX, SOCK_STREAM, 0, ctx.fd) != 0)
    goto socketpair_failed;

  stack = gum_alloc_n_pages (1, GUM_PAGE_RW);
  tls = gum_alloc_n_pages (1, GUM_PAGE_RW);

  desc = tls;


  /*
   * It seems like the only reliable way to read/write the registers of
   * another thread is to use ptrace(). We used to accomplish this by
   * hi-jacking the target thread by installing a signal handler and sending a
   * real-time signal directed at the target thread, and thus relying on the
   * signal handler getting called in that thread. The signal handler would
   * then provide us with read/write access to its registers. This hack would
   * however not work if a thread was for example blocking in poll(), as the
   * signal would then just get queued and we'd end up waiting indefinitely.
   *
   * It is however not possible to ptrace() another thread when we're in the
   * same process group. This used to be supported in old kernels, but it was
   * buggy and eventually dropped. So in order to use ptrace() we will need to
   * spawn a new thread in a different process group so that it can ptrace()
   * the target thread inside our process group. This is also the solution
   * recommended by Linus:
   *
   * https://lkml.org/lkml/2006/9/1/217
   *
   * Because libc implementations don't expose an API to do this, and the
   * thread setup code is private, where the TLS part is crucial for even just
   * the syscall wrappers - due to them accessing `errno` - we cannot make any
   * libc calls in this thread. And because the libc's clone() syscall wrapper
   * typically writes to the child thread's TLS structures, which we cannot
   * portably set up correctly, we cannot use the libc clone() syscall wrapper
   * either.
   */
  child = gum_libc_clone (
      gum_linux_do_modify_thread,
      stack + gum_query_page_size (),
      CLONE_VM | CLONE_SETTLS,
      &ctx,
      NULL,
      desc,
      NULL);
  if (child == -1)
    goto clone_failed;

  gum_acquire_dumpability ();

  prctl (PR_SET_PTRACER, child);

  if (thread_id == gum_process_get_current_thread_id ())
  {
    success = GPOINTER_TO_UINT (g_thread_join (g_thread_new (
            NULL,
            gum_linux_handle_modify_thread_comms,
            &ctx)));
  }
  else
  {
    success = GPOINTER_TO_UINT (gum_linux_handle_modify_thread_comms (&ctx));
  }

  gum_release_dumpability ();

  waitpid (child, NULL, __WCLONE);

  if (!success)
    goto attach_failed;

  goto beach;

socketpair_failed:
  {
    /* "Unable to create socketpair" */
    static const guint8 enc[] = {
      0xf2, 0xc9, 0xc6, 0xc5, 0xcb, 0xc2, 0x87, 0xd3, 0xc8, 0x87, 0xc4, 0xd5,
      0xc2, 0xc6, 0xd3, 0xc2, 0x87, 0xd4, 0xc8, 0xc4, 0xcc, 0xc2, 0xd3, 0xd7,
      0xc6, 0xce, 0xd5, 0xa7
    };
    char message[sizeof (enc)];

    sp_dec (message, enc, sizeof (enc));
    g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED, "%s", message);
    goto beach;
  }
clone_failed:
  {
    /* "Unable to set up clone" */
    static const guint8 enc[] = {
      0xf2, 0xc9, 0xc6, 0xc5, 0xcb, 0xc2, 0x87, 0xd3, 0xc8, 0x87, 0xd4, 0xc2,
      0xd3, 0x87, 0xd2, 0xd7, 0x87, 0xc4, 0xcb, 0xc8, 0xc9, 0xc2, 0xa7
    };
    char message[sizeof (enc)];

    sp_dec (message, enc, sizeof (enc));
    g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED, "%s", message);
    goto beach;
  }
attach_failed:
  {
    /* "Unable to PTRACE_ATTACH" */
    static const guint8 enc[] = {
      0xf2, 0xc9, 0xc6, 0xc5, 0xcb, 0xc2, 0x87, 0xd3, 0xc8, 0x87, 0xf7, 0xf3,
      0xf5, 0xe6, 0xe4, 0xe2, 0xf8, 0xe6, 0xf3, 0xf3, 0xe6, 0xe4, 0xef, 0xa7
    };
    char message[sizeof (enc)];

    sp_dec (message, enc, sizeof (enc));
    g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED, "%s", message);
    goto beach;
  }
beach:
  {
    g_clear_pointer (&tls, gum_free_pages);
    g_clear_pointer (&stack, gum_free_pages);

    if (ctx.fd[0] != -1)
      close (ctx.fd[0]);
    if (ctx.fd[1] != -1)
      close (ctx.fd[1]);

    return success;
  }
}

static gpointer
gum_linux_handle_modify_thread_comms (gpointer data)
{
  GumLinuxModifyThreadContext * ctx = data;
  gint fd = ctx->fd[0];
  gboolean success = FALSE;

  gum_put_ack (fd, GUM_ACK_READY);

  if (gum_await_ack (fd, GUM_ACK_READ_REGISTERS))
  {
    ctx->func (ctx->thread_id, &ctx->regs_data, ctx->user_data);
    gum_put_ack (fd, GUM_ACK_MODIFIED_REGISTERS);

    success = gum_await_ack (fd, GUM_ACK_WROTE_REGISTERS);
  }

  return GSIZE_TO_POINTER (success);
}

static gint
gum_linux_do_modify_thread (gpointer data)
{
  GumLinuxModifyThreadContext * ctx = data;
  gint fd;
  gboolean attached = FALSE;
  gssize res;
  pid_t wait_result;
  int status;
  struct user_hwdebug_state debug_regs;
  const guint debug_regs_type = (ctx->regs_type == GUM_REGS_DEBUG_BREAK)
      ? NT_ARM_HW_BREAK
      : NT_ARM_HW_WATCH;
  gsize debug_regs_size = sizeof (struct user_hwdebug_state);
  guint debug_regs_count = 0;
  guint i;

  fd = ctx->fd[1];

  gum_await_ack (fd, GUM_ACK_READY);

  res = gum_libc_ptrace (PTRACE_ATTACH, ctx->thread_id, NULL, NULL);
  if (res == -1)
    goto failed_to_attach;
  attached = TRUE;

  wait_result = gum_libc_waitpid (ctx->thread_id, &status, __WALL);

  if (wait_result != ctx->thread_id)
    goto failed_to_wait;

  if (!WIFSTOPPED (status))
    goto failed_to_stop;

  /*
   * Although ptrace injects SIGSTOP into our process, it is possible that our
   * target is stopped by another stop signal (e.g. SIGTTIN). The man pages for
   * ptrace mention the possible race condition. For our purposes, however, we
   * only require that the target is stopped so that we can read its registers.
   */
  if (ctx->regs_type == GUM_REGS_GENERAL_PURPOSE)
  {
    gsize regs_size = sizeof (GumGPRegs);

    res = gum_get_regs (ctx->thread_id, NT_PRSTATUS, &ctx->regs_data,
        &regs_size);
    if (res == -1)
      goto failed_to_read;
  }
  else
  {
    res = gum_get_regs (ctx->thread_id, debug_regs_type, &debug_regs,
        &debug_regs_size);
    if (res == -1)
      goto failed_to_read;

    debug_regs_count = debug_regs.dbg_info & 0xff;

    for (i = 0; i != G_N_ELEMENTS (debug_regs.dbg_regs); i++)
    {
      ctx->regs_data.debug.cr[i] = debug_regs.dbg_regs[i].ctrl;
      ctx->regs_data.debug.vr[i] = debug_regs.dbg_regs[i].addr;
    }
  }
  gum_put_ack (fd, GUM_ACK_READ_REGISTERS);

  gum_await_ack (fd, GUM_ACK_MODIFIED_REGISTERS);
  if (ctx->regs_type == GUM_REGS_GENERAL_PURPOSE)
  {
    res = gum_set_regs (ctx->thread_id, NT_PRSTATUS, &ctx->regs_data,
        sizeof (GumGPRegs));
    if (res == -1)
      goto failed_to_write;
  }
  else
  {
    for (i = 0; i != debug_regs_count; i++)
    {
      debug_regs.dbg_regs[i].ctrl = ctx->regs_data.debug.cr[i];
      debug_regs.dbg_regs[i].addr = ctx->regs_data.debug.vr[i];
    }

    res = gum_set_regs (ctx->thread_id, debug_regs_type, &debug_regs,
        G_STRUCT_OFFSET (struct user_hwdebug_state, dbg_regs) +
        debug_regs_count * 16);
    if (res == -1)
      goto failed_to_write;
  }

  res = gum_libc_ptrace (PTRACE_DETACH, ctx->thread_id, NULL,
      GINT_TO_POINTER (SIGCONT));

  attached = FALSE;
  if (res == -1)
    goto failed_to_detach;

  gum_put_ack (fd, GUM_ACK_WROTE_REGISTERS);

  goto beach;

failed_to_attach:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_ATTACH);
    goto beach;
  }
failed_to_wait:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_WAIT);
    goto beach;
  }
failed_to_stop:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_STOP);
    goto beach;
  }
failed_to_read:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_READ);
    goto beach;
  }
failed_to_write:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_WRITE);
    goto beach;
  }
failed_to_detach:
  {
    gum_put_ack (fd, GUM_ACK_FAILED_TO_DETACH);
    goto beach;
  }
beach:
  {
    if (attached)
    {
      gum_libc_ptrace (PTRACE_DETACH, ctx->thread_id, NULL,
          GINT_TO_POINTER (SIGCONT));
    }

    return 0;
  }
}

static gboolean
gum_await_ack (gint fd,
               GumModifyThreadAck expected_ack)
{
  guint8 value;
  gssize res;

  res = GUM_TEMP_FAILURE_RETRY (gum_libc_read (fd, &value, sizeof (value)));
  if (res == -1)
    return FALSE;

  return value == expected_ack;
}

static void
gum_put_ack (gint fd,
             GumModifyThreadAck ack)
{
  guint8 value;

  value = ack;
  GUM_TEMP_FAILURE_RETRY (gum_libc_write (fd, &value, sizeof (value)));
}

void
_gum_process_enumerate_threads (GumFoundThreadFunc func,
                                gpointer user_data)
{
  GDir * dir;
  const gchar * name;
  gboolean carry_on = TRUE;

  dir = g_dir_open ("/proc/self/task", 0, NULL);
  g_assert (dir != NULL);

  while (carry_on && (name = g_dir_read_name (dir)) != NULL)
  {
    GumThreadDetails details;
    gchar * thread_name;

    details.id = atoi (name);

    thread_name = gum_thread_read_name (details.id);
    details.name = thread_name;

    if (gum_thread_read_state (details.id, &details.state))
    {
      if (gum_process_modify_thread (details.id, gum_store_cpu_context,
            &details.cpu_context, GUM_MODIFY_THREAD_FLAGS_ABORT_SAFELY))
      {
        carry_on = func (&details, user_data);
      }
    }

    g_free (thread_name);
  }

  g_dir_close (dir);
}

static void
gum_store_cpu_context (GumThreadId thread_id,
                       GumCpuContext * cpu_context,
                       gpointer user_data)
{
  memcpy (user_data, cpu_context, sizeof (GumCpuContext));
}

gboolean
_gum_process_collect_main_module (GumModule * module,
                                  gpointer user_data)
{
  GumModule ** out = user_data;

  *out = g_object_ref (module);

  return FALSE;
}

GumModule *
gum_process_get_libc_module (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, (GThreadFunc) gum_try_init_libc_module, NULL);

  if (once.retval == NULL)
    {
      /* "Unable to locate the libc; please file a bug" */
      static const guint8 enc[] = {
        0xf2, 0xc9, 0xc6, 0xc5, 0xcb, 0xc2, 0x87, 0xd3, 0xc8, 0x87, 0xcb, 0xc8,
        0xc4, 0xc6, 0xd3, 0xc2, 0x87, 0xd3, 0xcf, 0xc2, 0x87, 0xcb, 0xce, 0xc5,
        0xc4, 0x9c, 0x87, 0xd7, 0xcb, 0xc2, 0xc6, 0xd4, 0xc2, 0x87, 0xc1, 0xce,
        0xcb, 0xc2, 0x87, 0xc6, 0x87, 0xc5, 0xd2, 0xc0, 0xa7
      };
      char message[sizeof (enc)];

      sp_dec (message, enc, sizeof (enc));
      gum_panic ("%s", message);
    }

  return once.retval;
}

static GumModule *
gum_try_init_libc_module (void)
{
  gum_libc_module = gum_process_find_module_by_address (
      GUM_ADDRESS (gum_process_get_libc_info ()->dli_fbase));

  _gum_register_destructor (gum_deinit_libc_module);

  return gum_libc_module;
}

static void
gum_deinit_libc_module (void)
{
  g_object_unref (gum_libc_module);
}

static const Dl_info *
gum_process_get_libc_info (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, (GThreadFunc) gum_try_init_libc_info, NULL);

  if (once.retval == NULL)
    {
      /* "Unable to locate the libc; please file a bug" */
      static const guint8 enc[] = {
        0xf2, 0xc9, 0xc6, 0xc5, 0xcb, 0xc2, 0x87, 0xd3, 0xc8, 0x87, 0xcb, 0xc8,
        0xc4, 0xc6, 0xd3, 0xc2, 0x87, 0xd3, 0xcf, 0xc2, 0x87, 0xcb, 0xce, 0xc5,
        0xc4, 0x9c, 0x87, 0xd7, 0xcb, 0xc2, 0xc6, 0xd4, 0xc2, 0x87, 0xc1, 0xce,
        0xcb, 0xc2, 0x87, 0xc6, 0x87, 0xc5, 0xd2, 0xc0, 0xa7
      };
      char message[sizeof (enc)];

      sp_dec (message, enc, sizeof (enc));
      gum_panic ("%s", message);
    }

  return once.retval;
}

static const Dl_info *
gum_try_init_libc_info (void)
{
  {
    if (!gum_try_resolve_dynamic_symbol ("exit", &gum_libc_info))
      return NULL;
  }

  return &gum_libc_info;
}

GumModule *
gum_process_load_module (const gchar * module_name,
                         GError ** error)
{
  GumModule * module;
  GumGenericDlopenImpl dlopen_impl = dlopen;
  gpointer handle;

  module = gum_process_find_module_by_name (module_name);
  if (module != NULL)
    return module;

  if (gum_android_get_linker_flavor () == GUM_ANDROID_LINKER_NATIVE)
    gum_android_find_unrestricted_dlopen (&dlopen_impl);

  handle = dlopen_impl (module_name, RTLD_LAZY);
  if (handle == NULL)
    goto not_found;

  module = gum_process_find_module_by_name (module_name);
  g_assert (module != NULL);

  dlclose (handle);

  return module;

not_found:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND, "%s", dlerror ());
    return FALSE;
  }
}

void
_gum_process_enumerate_modules (GumFoundModuleFunc func,
                                gpointer user_data)
{
  const GumProgramModules * pm;
  static gsize iterate_phdr_value = 0;
  GumDlIteratePhdrImpl iterate_phdr;

  pm = gum_query_program_modules ();

  if (pm->rtld == GUM_PROGRAM_RTLD_NONE)
  {
    if (!func (pm->program, user_data))
      return;

    if (pm->vdso != NULL)
      func (pm->vdso, user_data);

    return;
  }

  if (gum_android_get_linker_flavor () == GUM_ANDROID_LINKER_NATIVE)
  {
    gum_android_enumerate_modules (func, user_data);
    return;
  }

  if (g_once_init_enter (&iterate_phdr_value))
  {
    gpointer libc, impl;

    libc = dlopen (gum_process_get_libc_info ()->dli_fname,
        RTLD_LAZY | RTLD_GLOBAL);
    g_assert (libc != NULL);

    impl = dlsym (libc, "dl_iterate_phdr");

    dlclose (libc);

    g_once_init_leave (&iterate_phdr_value, GPOINTER_TO_SIZE (impl) + 1);
  }

  iterate_phdr = GSIZE_TO_POINTER (iterate_phdr_value - 1);
  if (iterate_phdr != NULL)
  {
    gum_process_enumerate_modules_by_using_libc (iterate_phdr, func, user_data);
  }
  else
  {
    gum_linux_enumerate_modules_using_proc_maps (func, user_data);
  }
}

static void
gum_process_enumerate_modules_by_using_libc (GumDlIteratePhdrImpl iterate_phdr,
                                             GumFoundModuleFunc func,
                                             gpointer user_data)
{
  GumEnumerateModulesContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.named_ranges = gum_linux_collect_named_ranges ();

  iterate_phdr (gum_emit_module_from_phdr, &ctx);

  g_hash_table_unref (ctx.named_ranges);
}

static gint
gum_emit_module_from_phdr (struct dl_phdr_info * info,
                           gsize size,
                           gpointer user_data)
{
  GumEnumerateModulesContext * ctx = user_data;
  GumMemoryRange range;
  GumLinuxNamedRange * named_range;
  const gchar * path;
  GumNativeModule * module;
  gboolean carry_on;

  gum_compute_elf_range_from_phdrs (info->dlpi_phdr, sizeof (ElfW(Phdr)),
      info->dlpi_phnum, 0, &range);

  named_range = g_hash_table_lookup (ctx->named_ranges,
      GSIZE_TO_POINTER (range.base_address));

  path = (named_range != NULL) ? named_range->name : info->dlpi_name;

  module = _gum_native_module_make (path, &range, gum_create_module_handle,
      NULL, NULL, (GDestroyNotify) dlclose);

  carry_on = ctx->func (GUM_MODULE (module), ctx->user_data);

  g_object_unref (module);

  return carry_on ? 0 : 1;
}

void
gum_linux_enumerate_modules_using_proc_maps (GumFoundModuleFunc func,
                                             gpointer user_data)
{
  GumProcMapsIter iter;
  gchar * path, * next_path;
  const gchar * line;
  gboolean carry_on = TRUE;
  gboolean got_line = FALSE;

  gum_proc_maps_iter_init_for_self (&iter);

  path = g_malloc (PATH_MAX);
  next_path = g_malloc (PATH_MAX);

  do
  {
    const guint8 elf_magic[] = { 0x7f, 'E', 'L', 'F' };
    GumMemoryRange range;
    GumAddress end;
    gchar perms[5] = { 0, };
    gint n;
    gboolean is_vdso, readable, shared;
    GumNativeModule * module;

    if (!got_line)
    {
      if (!gum_proc_maps_iter_next (&iter, &line))
        break;
    }
    else
    {
      got_line = FALSE;
    }

    n = sscanf (line,
        "%" G_GINT64_MODIFIER "x-%" G_GINT64_MODIFIER "x "
        "%4c "
        "%*x %*s %*d "
        "%[^\n]",
        &range.base_address, &end,
        perms,
        path);
    if (n == 3)
      continue;
    g_assert (n == 4);

    is_vdso = gum_try_translate_vdso_name (path);

    readable = perms[0] == 'r';
    shared = perms[3] == 's';
    if (!readable || shared)
      continue;
    else if ((path[0] != '/' && !is_vdso) || g_str_has_prefix (path, "/dev/"))
      continue;
    else if (RUNNING_ON_VALGRIND && strstr (path, "/valgrind/") != NULL)
      continue;
    else if (memcmp (GSIZE_TO_POINTER (range.base_address), elf_magic,
        sizeof (elf_magic)) != 0)
      continue;

    range.size = end - range.base_address;

    while (gum_proc_maps_iter_next (&iter, &line))
    {
      n = sscanf (line,
          "%*x-%" G_GINT64_MODIFIER "x %*c%*c%*c%*c %*x %*s %*d %[^\n]",
          &end,
          next_path);
      if (n == 1)
      {
        continue;
      }
      else if (n == 2 && next_path[0] == '[')
      {
        if (!gum_try_translate_vdso_name (next_path))
          continue;
      }

      if (n == 2 && strcmp (next_path, path) == 0)
      {
        range.size = end - range.base_address;
      }
      else
      {
        got_line = TRUE;
        break;
      }
    }

    module = _gum_native_module_make (path, &range, gum_create_module_handle,
        NULL, NULL, (GDestroyNotify) dlclose);

    carry_on = func (GUM_MODULE (module), user_data);

    g_object_unref (module);
  }
  while (carry_on);

  g_free (path);
  g_free (next_path);

  gum_proc_maps_iter_destroy (&iter);
}

static gpointer
gum_create_module_handle (GumNativeModule * module,
                          gpointer user_data)
{
  struct link_map * cur;

  for (cur = dlopen (NULL, 0); cur != NULL; cur = cur->l_next)
  {
    if (gum_linux_module_path_matches (cur->l_name, module->path))
      return cur;
  }

  for (cur = dlopen (NULL, 0); cur != NULL; cur = cur->l_next)
  {
    gchar * target, * parent_dir, * canonical_path;
    gboolean is_match;

    target = g_file_read_link (cur->l_name, NULL);
    if (target == NULL)
      continue;
    parent_dir = g_path_get_dirname (cur->l_name);
    canonical_path = g_canonicalize_filename (target, parent_dir);

    is_match = gum_linux_module_path_matches (canonical_path, module->path);

    g_free (canonical_path);
    g_free (parent_dir);
    g_free (target);

    if (is_match)
      return cur;
  }

  return NULL;
}

GHashTable *
gum_linux_collect_named_ranges (void)
{
  GHashTable * result;
  GumProcMapsIter iter;
  gchar * name, * next_name;
  const gchar * line;
  gboolean carry_on = TRUE;
  gboolean got_line = FALSE;

  result = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_linux_named_range_free);

  gum_proc_maps_iter_init_for_self (&iter);

  name = g_malloc (PATH_MAX);
  next_name = g_malloc (PATH_MAX);

  do
  {
    GumAddress start, end;
    gsize size;
    gint n;
    GumLinuxNamedRange * range;

    if (!got_line)
    {
      if (!gum_proc_maps_iter_next (&iter, &line))
        break;
    }
    else
    {
      got_line = FALSE;
    }

    n = sscanf (line,
        "%" G_GINT64_MODIFIER "x-%" G_GINT64_MODIFIER "x "
        "%*4c "
        "%*x %*s %*d "
        "%[^\n]",
        &start, &end,
        name);
    if (n == 2)
      continue;
    g_assert (n == 3);

    gum_try_translate_vdso_name (name);

    size = end - start;

    while (gum_proc_maps_iter_next (&iter, &line))
    {
      n = sscanf (line,
          "%*x-%" G_GINT64_MODIFIER "x %*c%*c%*c%*c %*x %*s %*d %[^\n]",
          &end,
          next_name);
      if (n == 1)
      {
        continue;
      }
      else if (n == 2 && next_name[0] == '[')
      {
        if (!gum_try_translate_vdso_name (next_name))
          continue;
      }

      if (n == 2 && strcmp (next_name, name) == 0)
      {
        size = end - start;
      }
      else
      {
        got_line = TRUE;
        break;
      }
    }

    range = g_slice_new (GumLinuxNamedRange);

    range->name = g_strdup (name);
    range->base = GSIZE_TO_POINTER (start);
    range->size = size;

    g_hash_table_insert (result, range->base, range);
  }
  while (carry_on);

  g_free (name);
  g_free (next_name);

  gum_proc_maps_iter_destroy (&iter);

  return result;
}

static void
gum_linux_named_range_free (GumLinuxNamedRange * range)
{
  g_free ((gpointer) range->name);

  g_slice_free (GumLinuxNamedRange, range);
}

static gboolean
gum_try_translate_vdso_name (gchar * name)
{
  if (strcmp (name, "[vdso]") == 0)
  {
    strcpy (name, "linux-vdso.so.1");
    return TRUE;
  }

  return FALSE;
}

void
_gum_process_enumerate_ranges (GumPageProtection prot,
                               GumFoundRangeFunc func,
                               gpointer user_data)
{
  gum_linux_enumerate_ranges (getpid (), prot, func, user_data);
}

void
gum_linux_enumerate_ranges (pid_t pid,
                            GumPageProtection prot,
                            GumFoundRangeFunc func,
                            gpointer user_data)
{
  GumProcMapsIter iter;
  gboolean carry_on = TRUE;
  const gchar * line;

  gum_proc_maps_iter_init_for_pid (&iter, pid);

  while (carry_on && gum_proc_maps_iter_next (&iter, &line))
  {
    GumRangeDetails details;
    GumMemoryRange range;
    GumFileMapping file;
    GumAddress end;
    gchar perms[5] = { 0, };
    guint64 inode;
    gint length;

    sscanf (line,
        "%" G_GINT64_MODIFIER "x-%" G_GINT64_MODIFIER "x "
        "%4c "
        "%" G_GINT64_MODIFIER "x %*s %" G_GINT64_MODIFIER "d"
        "%n",
        &range.base_address, &end,
        perms,
        &file.offset, &inode,
        &length);

    range.size = end - range.base_address;

    details.file = NULL;
    if (inode != 0)
    {
      file.path = strchr (line + length, '/');
      if (file.path != NULL)
      {
        details.file = &file;
        file.size = 0; /* TODO */

        if (RUNNING_ON_VALGRIND && strstr (file.path, "/valgrind/") != NULL)
          continue;
      }
    }

    details.range = &range;
    details.protection = gum_page_protection_from_proc_perms_string (perms);

    if ((details.protection & prot) == prot)
    {
      carry_on = func (&details, user_data);
    }
  }

  gum_proc_maps_iter_destroy (&iter);
}

void
gum_process_enumerate_malloc_ranges (GumFoundMallocRangeFunc func,
                                     gpointer user_data)
{
  /* Not implemented */
}

guint
gum_thread_try_get_ranges (GumMemoryRange * ranges,
                           guint max_length)
{
  guint n = 0;
  pthread_attr_t attr;
  gboolean allocated = FALSE;
  void * stack_addr;
  size_t stack_size;
  GumMemoryRange * range;

  if (pthread_getattr_np (pthread_self (), &attr) != 0)
    goto beach;
  allocated = TRUE;

  if (pthread_attr_getstack (&attr, &stack_addr, &stack_size) != 0)
    goto beach;

  range = &ranges[0];
  range->base_address = GUM_ADDRESS (stack_addr);
  range->size = stack_size;

  n = 1;

beach:
  if (allocated)
    pthread_attr_destroy (&attr);

  return n;
}

gint
gum_thread_get_system_error (void)
{
  return errno;
}

void
gum_thread_set_system_error (gint value)
{
  errno = value;
}

gboolean
gum_thread_suspend (GumThreadId thread_id,
                    GError ** error)
{
  if (syscall (__NR_tgkill, getpid (), thread_id, SIGSTOP) != 0)
    goto failure;

  return TRUE;

failure:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED, "%s", g_strerror (errno));
    return FALSE;
  }
}

gboolean
gum_thread_resume (GumThreadId thread_id,
                   GError ** error)
{
  if (syscall (__NR_tgkill, getpid (), thread_id, SIGCONT) != 0)
    goto failure;

  return TRUE;

failure:
  {
    g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED, "%s", g_strerror (errno));
    return FALSE;
  }
}

gboolean
gum_thread_set_hardware_breakpoint (GumThreadId thread_id,
                                    guint breakpoint_id,
                                    GumAddress address,
                                    GError ** error)
{
  GumSetHardwareBreakpointContext bpc;

  bpc.breakpoint_id = breakpoint_id;
  bpc.address = address;

  gum_linux_modify_thread (thread_id, GUM_REGS_DEBUG_BREAK,
      gum_do_set_hardware_breakpoint, &bpc, error);

  return gum_linux_modify_thread (thread_id, GUM_REGS_DEBUG_BREAK,
      gum_do_set_hardware_breakpoint, &bpc, error);
}

static void
gum_do_set_hardware_breakpoint (GumThreadId thread_id,
                                GumRegs * regs,
                                gpointer user_data)
{
  GumDebugRegs * dr = &regs->debug;
  GumSetHardwareBreakpointContext * bpc = user_data;

  _gum_arm64_set_breakpoint (dr->cr, dr->vr, bpc->breakpoint_id, bpc->address);
}

gboolean
gum_thread_unset_hardware_breakpoint (GumThreadId thread_id,
                                      guint breakpoint_id,
                                      GError ** error)
{
  return gum_linux_modify_thread (thread_id, GUM_REGS_DEBUG_BREAK,
      gum_do_unset_hardware_breakpoint, GUINT_TO_POINTER (breakpoint_id),
      error);
}

static void
gum_do_unset_hardware_breakpoint (GumThreadId thread_id,
                                  GumRegs * regs,
                                  gpointer user_data)
{
  GumDebugRegs * dr = &regs->debug;
  guint breakpoint_id = GPOINTER_TO_UINT (user_data);

  _gum_arm64_unset_breakpoint (dr->cr, dr->vr, breakpoint_id);
}

gboolean
gum_thread_set_hardware_watchpoint (GumThreadId thread_id,
                                    guint watchpoint_id,
                                    GumAddress address,
                                    gsize size,
                                    GumWatchConditions wc,
                                    GError ** error)
{
  GumSetHardwareWatchpointContext wpc;

  wpc.watchpoint_id = watchpoint_id;
  wpc.address = address;
  wpc.size = size;
  wpc.conditions = wc;

  return gum_linux_modify_thread (thread_id, GUM_REGS_DEBUG_WATCH,
      gum_do_set_hardware_watchpoint, &wpc, error);
}

static void
gum_do_set_hardware_watchpoint (GumThreadId thread_id,
                                GumRegs * regs,
                                gpointer user_data)
{
  GumDebugRegs * dr = &regs->debug;
  GumSetHardwareWatchpointContext * wpc = user_data;

  _gum_arm64_set_watchpoint (dr->cr, dr->vr, wpc->watchpoint_id, wpc->address,
      wpc->size, wpc->conditions);
}

gboolean
gum_thread_unset_hardware_watchpoint (GumThreadId thread_id,
                                      guint watchpoint_id,
                                      GError ** error)
{
  return gum_linux_modify_thread (thread_id, GUM_REGS_DEBUG_WATCH,
      gum_do_unset_hardware_watchpoint, GUINT_TO_POINTER (watchpoint_id),
      error);
}

static void
gum_do_unset_hardware_watchpoint (GumThreadId thread_id,
                                  GumRegs * regs,
                                  gpointer user_data)
{
  GumDebugRegs * dr = &regs->debug;
  guint watchpoint_id = GPOINTER_TO_UINT (user_data);

  _gum_arm64_unset_watchpoint (dr->cr, dr->vr, watchpoint_id);
}

gboolean
gum_linux_check_kernel_version (guint major,
                                guint minor,
                                guint micro)
{
  static gboolean initialized = FALSE;
  static guint kern_major = G_MAXUINT;
  static guint kern_minor = G_MAXUINT;
  static guint kern_micro = G_MAXUINT;

  if (!initialized)
  {
    struct utsname un;
    G_GNUC_UNUSED int res;

    res = uname (&un);
    g_assert (res == 0);

    sscanf (un.release, "%u.%u.%u", &kern_major, &kern_minor, &kern_micro);

    initialized = TRUE;
  }

  if (kern_major > major)
    return TRUE;

  if (kern_major == major && kern_minor > minor)
    return TRUE;

  return kern_major == major && kern_minor == minor && kern_micro >= micro;
}

GumCpuType
gum_linux_cpu_type_from_file (const gchar * path,
                              GError ** error)
{
  GumCpuType result = -1;
  FILE * file;
  guint8 ei_data;
  guint16 e_machine;

  file = fopen (path, "rb");
  if (file == NULL)
    goto fopen_failed;

  if (fseek (file, EI_DATA, SEEK_SET) != 0)
    goto unsupported_executable;
  if (fread (&ei_data, sizeof (ei_data), 1, file) != 1)
    goto unsupported_executable;

  if (fseek (file, 0x12, SEEK_SET) != 0)
    goto unsupported_executable;
  if (fread (&e_machine, sizeof (e_machine), 1, file) != 1)
    goto unsupported_executable;

  if (ei_data == ELFDATA2LSB)
    e_machine = GUINT16_FROM_LE (e_machine);
  else if (ei_data == ELFDATA2MSB)
    e_machine = GUINT16_FROM_BE (e_machine);
  else
    goto unsupported_ei_data;

  switch (e_machine)
  {
    case 0x0003:
      result = GUM_CPU_IA32;
      break;
    case 0x003e:
      result = GUM_CPU_AMD64;
      break;
    case 0x0028:
      result = GUM_CPU_ARM;
      break;
    case 0x00b7:
      result = GUM_CPU_ARM64;
      break;
    case 0x0008:
      result = GUM_CPU_MIPS;
      break;
    default:
      goto unsupported_executable;
  }

  goto beach;

fopen_failed:
  {
    if (errno == ENOENT)
    {
      /* "File not found" */
      static const unsigned char enc[] = {
        0xe1, 0xce, 0xcb, 0xc2, 0x87, 0xc9, 0xc8, 0xd3, 0x87, 0xc1, 0xc8, 0xd2,
        0xc9, 0xc3, 0xa7
      };
      char message[sizeof (enc)];

      sp_dec (message, enc, sizeof (enc));
      g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND, "%s", message);
    }
    else if (errno == EACCES)
    {
      /* "Permission denied" */
      static const unsigned char enc[] = {
        0xf7, 0xc2, 0xd5, 0xca, 0xce, 0xd4, 0xd4, 0xce, 0xc8, 0xc9, 0x87, 0xc3,
        0xc2, 0xc9, 0xce, 0xc2, 0xc3, 0xa7
      };
      char message[sizeof (enc)];

      sp_dec (message, enc, sizeof (enc));
      g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED, "%s", message);
    }
    else
    {
      /* "Unable to open file: %s" */
      static const unsigned char enc[] = {
        0xf2, 0xc9, 0xc6, 0xc5, 0xcb, 0xc2, 0x87, 0xd3, 0xc8, 0x87, 0xc8, 0xd7,
        0xc2, 0xc9, 0x87, 0xc1, 0xce, 0xcb, 0xc2, 0x9d, 0x87, 0x82, 0xd4, 0xa7
      };
      char message[sizeof (enc)];

      sp_dec (message, enc, sizeof (enc));
      g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED, message,
          g_strerror (errno));
    }
    goto beach;
  }
unsupported_ei_data:
  {
    /* "Unsupported ELF EI_DATA" */
    static const unsigned char enc[] = {
      0xf2, 0xc9, 0xd4, 0xd2, 0xd7, 0xd7, 0xc8, 0xd5, 0xd3, 0xc2, 0xc3, 0x87,
      0xe2, 0xeb, 0xe1, 0x87, 0xe2, 0xee, 0xf8, 0xe3, 0xe6, 0xf3, 0xe6, 0xa7
    };
    char message[sizeof (enc)];

    sp_dec (message, enc, sizeof (enc));
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED, "%s", message);
    goto beach;
  }
unsupported_executable:
  {
    /* "Unsupported executable" */
    static const unsigned char enc[] = {
      0xf2, 0xc9, 0xd4, 0xd2, 0xd7, 0xd7, 0xc8, 0xd5, 0xd3, 0xc2, 0xc3, 0x87,
      0xc2, 0xdf, 0xc2, 0xc4, 0xd2, 0xd3, 0xc6, 0xc5, 0xcb, 0xc2, 0xa7
    };
    char message[sizeof (enc)];

    sp_dec (message, enc, sizeof (enc));
    g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_SUPPORTED, "%s", message);
    goto beach;
  }
beach:
  {
    if (file != NULL)
      fclose (file);

    return result;
  }
}

static void
gum_set_process_not_found (GError ** error)
{
  /* "Process not found", XOR 0xA7 */
  static const unsigned char enc[] = {
    0xf7, 0xd5, 0xc8, 0xc4, 0xc2, 0xd4, 0xd4, 0x87, 0xc9, 0xc8, 0xd3, 0x87, 0xc1,
    0xc8, 0xd2, 0xc9, 0xc3, 0xa7
  };
  char message[sizeof (enc)];

  sp_dec (message, enc, sizeof (enc));
  g_set_error (error, GUM_ERROR, GUM_ERROR_NOT_FOUND, "%s", message);
}

GumCpuType
gum_linux_cpu_type_from_pid (pid_t pid,
                             GError ** error)
{
  GumCpuType result = -1;
  GError * err;
  gchar * auxv_path, * auxv;
  gsize auxv_size;

  auxv_path = g_strdup_printf ("/proc/%d/auxv", pid);

  auxv = NULL;
  err = NULL;
  if (!g_file_get_contents (auxv_path, &auxv, &auxv_size, &err))
    goto read_failed;
  if (auxv_size == 0)
    goto nearly_dead;

  result = gum_linux_cpu_type_from_auxv (auxv, auxv_size);

  goto beach;

read_failed:
  {
    if (g_error_matches (err, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    {
      gum_set_process_not_found (error);
    }
    else if (g_error_matches (err, G_FILE_ERROR, G_FILE_ERROR_ACCES))
    {
      /* "Permission denied" */
      {
        static const unsigned char enc[] = {
          0xf7, 0xc2, 0xd5, 0xca, 0xce, 0xd4, 0xd4, 0xce, 0xc8, 0xc9, 0x87, 0xc3,
          0xc2, 0xc9, 0xce, 0xc2, 0xc3, 0xa7
        };
        char message[sizeof (enc)];

        sp_dec (message, enc, sizeof (enc));
        g_set_error (error, GUM_ERROR, GUM_ERROR_PERMISSION_DENIED, "%s",
            message);
      }
    }
    else
    {
      g_set_error (error, GUM_ERROR, GUM_ERROR_FAILED,
          "%s", err->message);
    }

    g_error_free (err);

    goto beach;
  }
nearly_dead:
  {
    gum_set_process_not_found (error);
    goto beach;
  }
beach:
  {
    g_free (auxv);
    g_free (auxv_path);

    return result;
  }
}

GumCpuType
gum_linux_cpu_type_from_auxv (gconstpointer auxv,
                              gsize auxv_size)
{
  GumCpuType result = -1;
  GumCpuType cpu32, cpu64;
  gsize i;

  cpu32 = GUM_CPU_ARM;
  cpu64 = GUM_CPU_ARM64;

  /*
   * The auxilliary structure format is architecture specific. Most notably,
   * type and value are both natively sized. We therefore detect whether a
   * process is 64-bit by examining each entry and confirming that the low bits
   * of the type field are zero. Note that this is itself endian specific.
   *
   * typedef struct
   * {
   *   uint32_t a_type;
   *   union
   *   {
   *     uint32_t a_val;
   *   } a_un;
   * } Elf32_auxv_t;
   *
   * typedef struct
   * {
   *   uint64_t a_type;
   *   union
   *   {
   *     uint64_t a_val;
   *   } a_un;
   * } Elf64_auxv_t;
   *
   * If the auxiliary vector is 32-bits and contains only an AT_NULL entry (note
   * that the documentation states that "The last entry contains two zeros"),
   * this will mean it has no non-zero type codes and could be mistaken for a
   * 64-bit format auxiliary vector. We therefore handle this special case.
   *
   * If the vector is less than 16 bytes it is not large enough to contain two
   * 64-bit zero values. If it is larger, then if it is a 32-bit format vector,
   * then it must contain at least one non-zero type code and hence the test
   * below should work.
   */

  if (auxv_size < 2 * sizeof (guint64))
  {
    result = cpu32;
  }
  else
  {
    result = cpu64;

    for (i = 0; i + sizeof (guint64) <= auxv_size; i += 16)
    {
      const guint64 * auxv_type = auxv + i;

      if ((*auxv_type & G_GUINT64_CONSTANT (0xffffffff00000000)) != 0)
      {
        result = cpu32;
        break;
      }
    }
  }

  return result;
}

gboolean
gum_linux_module_path_matches (const gchar * path,
                               const gchar * name_or_path)
{
  const gchar * s;

  if (name_or_path[0] == '/')
    return strcmp (name_or_path, path) == 0;

  if ((s = strrchr (path, '/')) != NULL)
    return strcmp (name_or_path, s + 1) == 0;

  return strcmp (name_or_path, path) == 0;
}

void
gum_proc_maps_iter_init_for_self (GumProcMapsIter * iter)
{
  gum_proc_maps_iter_init_for_path (iter, "/proc/self/maps");
}

void
gum_proc_maps_iter_init_for_pid (GumProcMapsIter * iter,
                                 pid_t pid)
{
  gchar path[31 + 1];

  sprintf (path, "/proc/%u/maps", (guint) pid);

  gum_proc_maps_iter_init_for_path (iter, path);
}

static void
gum_proc_maps_iter_init_for_path (GumProcMapsIter * iter,
                                  const gchar * path)
{
  iter->fd = open (path, O_RDONLY | O_CLOEXEC);
  iter->read_cursor = iter->buffer;
  iter->write_cursor = iter->buffer;
}

void
gum_proc_maps_iter_destroy (GumProcMapsIter * iter)
{
  if (iter->fd != -1)
    close (iter->fd);
}

gboolean
gum_proc_maps_iter_next (GumProcMapsIter * iter,
                         const gchar ** line)
{
  gchar * next_newline;
  guint available;
  gboolean need_refill;

  if (iter->fd == -1)
    return FALSE;

  next_newline = NULL;

  available = iter->write_cursor - iter->read_cursor;
  if (available == 0)
  {
    need_refill = TRUE;
  }
  else
  {
    next_newline = strchr (iter->read_cursor, '\n');
    if (next_newline != NULL)
    {
      need_refill = FALSE;
    }
    else
    {
      need_refill = TRUE;
    }
  }

  if (need_refill)
  {
    guint offset;
    gssize res;

    offset = iter->read_cursor - iter->buffer;
    if (offset > 0)
    {
      memmove (iter->buffer, iter->read_cursor, available);
      iter->read_cursor -= offset;
      iter->write_cursor -= offset;
    }

    res = GUM_TEMP_FAILURE_RETRY (gum_libc_read (iter->fd,
        iter->write_cursor,
        iter->buffer + sizeof (iter->buffer) - 1 - iter->write_cursor));
    if (res <= 0)
      return FALSE;

    iter->write_cursor += res;
    iter->write_cursor[0] = '\0';

    next_newline = strchr (iter->read_cursor, '\n');
  }

  *line = iter->read_cursor;
  *next_newline = '\0';

  iter->read_cursor = next_newline + 1;

  return TRUE;
}

static void
gum_acquire_dumpability (void)
{
  G_LOCK (gum_dumpable);

  if (++gum_dumpable_refcount == 1)
  {
    /*
     * Some systems (notably Android on release applications) spawn processes as
     * not dumpable by default, disabling ptrace() and some other things on that
     * process for anyone other than root.
     */
    gum_dumpable_previous = prctl (PR_GET_DUMPABLE);
    if (gum_dumpable_previous != -1 && gum_dumpable_previous != 1)
      prctl (PR_SET_DUMPABLE, 1);
  }

  G_UNLOCK (gum_dumpable);
}

static void
gum_release_dumpability (void)
{
  G_LOCK (gum_dumpable);

  if (--gum_dumpable_refcount == 0)
  {
    if (gum_dumpable_previous != -1 && gum_dumpable_previous != 1)
      prctl (PR_SET_DUMPABLE, gum_dumpable_previous);
  }

  G_UNLOCK (gum_dumpable);
}

void
gum_linux_parse_ucontext (const ucontext_t * uc,
                          GumCpuContext * ctx)
{
  const mcontext_t * mc = &uc->uc_mcontext;
  gsize i;

  ctx->pc = mc->pc;
  ctx->sp = mc->sp;
  ctx->nzcv = 0;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    ctx->x[i] = mc->regs[i];
  ctx->fp = mc->regs[29];
  ctx->lr = mc->regs[30];

  memset (ctx->v, 0, sizeof (ctx->v));
}

void
gum_linux_unparse_ucontext (const GumCpuContext * ctx,
                            ucontext_t * uc)
{
  mcontext_t * mc = &uc->uc_mcontext;
  gsize i;

  mc->pc = ctx->pc;
  mc->sp = ctx->sp;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    mc->regs[i] = ctx->x[i];
  mc->regs[29] = ctx->fp;
  mc->regs[30] = ctx->lr;
}

static void
gum_parse_gp_regs (const GumGPRegs * regs,
                   GumCpuContext * ctx)
{
  gsize i;

  ctx->pc = regs->pc;
  ctx->sp = regs->sp;
  ctx->nzcv = 0;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    ctx->x[i] = regs->regs[i];
  ctx->fp = regs->regs[29];
  ctx->lr = regs->regs[30];

  memset (ctx->v, 0, sizeof (ctx->v));
}

static void
gum_unparse_gp_regs (const GumCpuContext * ctx,
                     GumGPRegs * regs)
{
  gsize i;

  regs->pc = ctx->pc;
  regs->sp = ctx->sp;

  for (i = 0; i != G_N_ELEMENTS (ctx->x); i++)
    regs->regs[i] = ctx->x[i];
  regs->regs[29] = ctx->fp;
  regs->regs[30] = ctx->lr;
}

static gchar *
gum_thread_read_name (GumThreadId thread_id)
{
  gchar * name = NULL;
  gchar * path;
  gchar * comm = NULL;

  path = g_strdup_printf ("/proc/self/task/%" G_GSIZE_FORMAT "/comm",
      thread_id);
  if (!g_file_get_contents (path, &comm, NULL, NULL))
    goto beach;
  name = g_strchomp (g_steal_pointer (&comm));

beach:
  g_free (comm);
  g_free (path);

  return name;
}

static gboolean
gum_thread_read_state (GumThreadId tid,
                       GumThreadState * state)
{
  gboolean success = FALSE;
  gchar * path, * info = NULL;

  path = g_strdup_printf ("/proc/self/task/%" G_GSIZE_FORMAT "/stat", tid);
  if (g_file_get_contents (path, &info, NULL, NULL))
  {
    gchar * p;

    p = strrchr (info, ')') + 2;

    *state = gum_thread_state_from_proc_status_character (*p);
    success = TRUE;
  }

  g_free (info);
  g_free (path);

  return success;
}

static GumThreadState
gum_thread_state_from_proc_status_character (gchar c)
{
  switch (g_ascii_toupper (c))
  {
    case 'R': return GUM_THREAD_RUNNING;
    case 'S': return GUM_THREAD_WAITING;
    case 'D': return GUM_THREAD_UNINTERRUPTIBLE;
    case 'Z': return GUM_THREAD_UNINTERRUPTIBLE;
    case 'T': return GUM_THREAD_STOPPED;
    case 'W':
    default:
      return GUM_THREAD_UNINTERRUPTIBLE;
  }
}

static GumPageProtection
gum_page_protection_from_proc_perms_string (const gchar * perms)
{
  GumPageProtection prot = GUM_PAGE_NO_ACCESS;

  if (perms[0] == 'r')
    prot |= GUM_PAGE_READ;
  if (perms[1] == 'w')
    prot |= GUM_PAGE_WRITE;
  if (perms[2] == 'x')
    prot |= GUM_PAGE_EXECUTE;

  return prot;
}

static gssize
gum_get_regs (pid_t pid,
              guint type,
              gpointer data,
              gsize * size)
{
  if (gum_is_regset_supported)
  {
    struct iovec io = {
      .iov_base = data,
      .iov_len = *size
    };
    gssize ret = gum_libc_ptrace (PTRACE_GETREGSET, pid,
        GUINT_TO_POINTER (type), &io);
    if (ret >= 0)
    {
      *size = io.iov_len;
      return ret;
    }
    if (ret == -EPERM || ret == -ESRCH)
      return ret;
    gum_is_regset_supported = FALSE;
  }

  return gum_libc_ptrace (PTRACE_GETREGS, pid, NULL, data);
}

static gssize
gum_set_regs (pid_t pid,
              guint type,
              gconstpointer data,
              gsize size)
{
  if (gum_is_regset_supported)
  {
    struct iovec io = {
      .iov_base = (void *) data,
      .iov_len = size
    };
    gssize ret = gum_libc_ptrace (PTRACE_SETREGSET, pid,
        GUINT_TO_POINTER (type), &io);
    if (ret >= 0)
      return ret;
    if (ret == -EPERM || ret == -ESRCH)
      return ret;
    gum_is_regset_supported = FALSE;
  }

  return gum_libc_ptrace (PTRACE_SETREGS, pid, NULL, (gpointer) data);
}

static gssize
gum_libc_clone (GumCloneFunc child_func,
                gpointer child_stack,
                gint flags,
                gpointer arg,
                pid_t * parent_tidptr,
                GumUserDesc * tls,
                pid_t * child_tidptr)
{
  gssize result;
  gpointer * child_sp = child_stack;

  *(--child_sp) = child_func;
  *(--child_sp) = arg;

  {
    register        gssize x8 asm ("x8") = __NR_clone;
    register          gint x0 asm ("x0") = flags;
    register    gpointer * x1 asm ("x1") = child_sp;
    register       pid_t * x2 asm ("x2") = parent_tidptr;
    register GumUserDesc * x3 asm ("x3") = tls;
    register       pid_t * x4 asm ("x4") = child_tidptr;

    asm volatile (
        "svc 0x0\n\t"
        "cbnz x0, 1f\n\t"

        /* child: */
        "ldp x0, x1, [sp], #16\n\t"
        "blr x1\n\t"
        "mov x8, %x[exit_syscall]\n\t"
        "svc 0x0\n\t"

        /* parent: */
        "1:\n\t"
        : "+r" (x0)
        : "r" (x1),
          "r" (x2),
          "r" (x3),
          "r" (x4),
          "r" (x8),
          [exit_syscall] "i" (__NR_exit)
        : "cc", "memory"
    );

    result = x0;
  }

  return result;
}

static gssize
gum_libc_read (gint fd,
               gpointer buf,
               gsize count)
{
  return gum_libc_syscall_3 (__NR_read, fd, GPOINTER_TO_SIZE (buf), count);
}

static gssize
gum_libc_write (gint fd,
                gconstpointer buf,
                gsize count)
{
  return gum_libc_syscall_3 (__NR_write, fd, GPOINTER_TO_SIZE (buf), count);
}

static pid_t
gum_libc_waitpid (pid_t pid,
                  int * status,
                  int options)
{
#ifdef __NR_waitpid
  return gum_libc_syscall_3 (__NR_waitpid, pid, GPOINTER_TO_SIZE (status),
      options);
#else
  return gum_libc_syscall_4 (__NR_wait4, pid, GPOINTER_TO_SIZE (status),
      options, 0);
#endif
}

static gssize
gum_libc_ptrace (gsize request,
                 pid_t pid,
                 gpointer address,
                 gpointer data)
{
  return gum_libc_syscall_4 (__NR_ptrace, request, pid,
      GPOINTER_TO_SIZE (address), GPOINTER_TO_SIZE (data));
}

static gssize
gum_libc_syscall_4 (gsize n,
                    gsize a,
                    gsize b,
                    gsize c,
                    gsize d)
{
  gssize result;

  {
    register gssize x8 asm ("x8") = n;
    register  gsize x0 asm ("x0") = a;
    register  gsize x1 asm ("x1") = b;
    register  gsize x2 asm ("x2") = c;
    register  gsize x3 asm ("x3") = d;

    asm volatile (
        "svc 0x0\n\t"
        : "+r" (x0)
        : "r" (x1),
          "r" (x2),
          "r" (x3),
          "r" (x8)
        : "memory"
    );

    result = x0;
  }

  return result;
}
