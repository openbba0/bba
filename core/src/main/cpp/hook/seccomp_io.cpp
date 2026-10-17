#include "seccomp_io.h"
#include "io.h"
#include "log.h"
#include "xdl.h"
#include "hook/gum_engine.h"

#include <cerrno>
#include <cstring>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef __NR_openat2
#define __NR_openat2 437
#endif
#ifndef __NR_faccessat2
#define __NR_faccessat2 439
#endif
#ifndef __NR_renameat2
#define __NR_renameat2 276
#endif
#ifndef __NR_statx
#define __NR_statx 291
#endif
#if !defined(__NR_fstatat) && defined(__NR_newfstatat)
#define __NR_fstatat __NR_newfstatat
#endif
#ifndef __NR_fstatat
#define __NR_fstatat 79
#endif

#if !defined(__aarch64__)
#error "seccomp_io is arm64-only"
#endif

#define SECCOMP_AUDIT_ARCH AUDIT_ARCH_AARCH64

static bool g_installed = false;
static bool g_lock_sigsys = false;
static int (*orig_sigaction)(int, const struct sigaction *, struct sigaction *) = nullptr;
static int (*orig_sigaction64)(int, const void *, void *) = nullptr;
static thread_local int g_in_handler = 0;
static thread_local char g_path_a[PATH_MAX];
static thread_local char g_path_b[PATH_MAX];

// Executable page that re-issues syscalls without re-trapping.
static uint8_t *g_safe_page = nullptr;
static long (*g_safe_syscall6)(long, long, long, long, long, long, long) = nullptr;

// AArch64: mov x8,x0; mov x0,x1; mov x1,x2; mov x2,x3; mov x3,x4; mov x4,x5; mov x5,x6; svc #0; ret
// args: n, a0, a1, a2, a3, a4, a5 in x0..x6 on entry.
static const uint32_t kSafeSyscallStub[] = {
        0xAA0003E8, // mov x8, x0
        0xAA0103E0, // mov x0, x1
        0xAA0203E1, // mov x1, x2
        0xAA0303E2, // mov x2, x3
        0xAA0403E3, // mov x3, x4
        0xAA0503E4, // mov x4, x5
        0xAA0603E5, // mov x5, x6
        0xD4000001, // svc #0
        0xD65F03C0, // ret
};

static bool ensure_safe_page() {
    if (g_safe_page != nullptr) {
        return true;
    }
    void *page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (page == MAP_FAILED) {
        ALOGE("SeccompIo: mmap safe page failed: %s", strerror(errno));
        return false;
    }
    memcpy(page, kSafeSyscallStub, sizeof(kSafeSyscallStub));
    if (mprotect(page, 4096, PROT_READ | PROT_EXEC) != 0) {
        ALOGE("SeccompIo: mprotect safe page failed: %s", strerror(errno));
        munmap(page, 4096);
        return false;
    }
    g_safe_page = static_cast<uint8_t *>(page);
    g_safe_syscall6 = reinterpret_cast<long (*)(long, long, long, long, long, long, long)>(page);
    __builtin___clear_cache(reinterpret_cast<char *>(page),
                            reinterpret_cast<char *>(page) + sizeof(kSafeSyscallStub));
    return true;
}

static bool is_path_syscall(long nr) {
    switch (nr) {
        case __NR_openat:
        case __NR_openat2:
        case __NR_faccessat:
        case __NR_faccessat2:
        case __NR_fstatat:
        case __NR_statx:
        case __NR_renameat:
        case __NR_renameat2:
        case __NR_unlinkat:
        case __NR_mkdirat:
        case __NR_readlinkat:
        case __NR_fchmodat:
        case __NR_fchownat:
        case __NR_linkat:
        case __NR_symlinkat:
        case __NR_mknodat:
            return true;
        default:
            return false;
    }
}

static const char *rewrite_path(const char *path, char *buf, size_t buflen) {
    if (path == nullptr) {
        return path;
    }
    const char *redirected = IO::redirectPath(path);
    if (redirected == nullptr || redirected == path) {
        return path;
    }
    size_t n = strlen(redirected);
    if (n >= buflen) {
        n = buflen - 1;
    }
    memcpy(buf, redirected, n);
    buf[n] = '\0';
    return buf;
}

static const char *path_or_null(long p) {
    return p == 0 ? "(null)" : reinterpret_cast<const char *>(p);
}

static const char *nr_name(long nr) {
    switch (nr) {
        case __NR_openat:
            return "openat";
        case __NR_openat2:
            return "openat2";
        case __NR_faccessat:
            return "faccessat";
        case __NR_faccessat2:
            return "faccessat2";
        case __NR_fstatat:
            return "fstatat";
        case __NR_statx:
            return "statx";
        case __NR_renameat:
            return "renameat";
        case __NR_renameat2:
            return "renameat2";
        case __NR_unlinkat:
            return "unlinkat";
        case __NR_mkdirat:
            return "mkdirat";
        case __NR_readlinkat:
            return "readlinkat";
        case __NR_fchmodat:
            return "fchmodat";
        case __NR_fchownat:
            return "fchownat";
        case __NR_linkat:
            return "linkat";
        case __NR_symlinkat:
            return "symlinkat";
        case __NR_mknodat:
            return "mknodat";
        default:
            return "path_svc";
    }
}

static void log_svc_args(long nr, long a0, long a1, long a2, long a3, long a4, long a5, bool before) {
    const char *tag = before ? "in" : "out";
    switch (nr) {
        case __NR_openat:
        case __NR_openat2:
            ALOGD("SeccompIo: %s %s dirfd=%ld path=%s flags=0x%lx mode/a3=0x%lx a4=%ld a5=%ld",
                  tag, nr_name(nr), a0, path_or_null(a1), a2, a3, a4, a5);
            break;
        case __NR_faccessat:
        case __NR_faccessat2:
            ALOGD("SeccompIo: %s %s dirfd=%ld path=%s mode=%ld flags=%ld",
                  tag, nr_name(nr), a0, path_or_null(a1), a2, a3);
            break;
        case __NR_fstatat:
        case __NR_statx:
        case __NR_fchmodat:
        case __NR_fchownat:
        case __NR_mknodat:
        case __NR_unlinkat:
        case __NR_mkdirat:
        case __NR_readlinkat:
            ALOGD("SeccompIo: %s %s dirfd=%ld path=%s a2=%ld a3=%ld a4=%ld a5=%ld",
                  tag, nr_name(nr), a0, path_or_null(a1), a2, a3, a4, a5);
            break;
        case __NR_symlinkat:
            ALOGD("SeccompIo: %s %s target=%s newdirfd=%ld linkpath=%s",
                  tag, nr_name(nr), path_or_null(a0), a1, path_or_null(a2));
            break;
        case __NR_linkat:
        case __NR_renameat:
        case __NR_renameat2:
            ALOGD("SeccompIo: %s %s olddirfd=%ld old=%s newdirfd=%ld new=%s a4=%ld a5=%ld",
                  tag, nr_name(nr), a0, path_or_null(a1), a2, path_or_null(a3), a4, a5);
            break;
        default:
            ALOGD("SeccompIo: %s nr=%ld a0=%ld a1=%ld a2=%ld a3=%ld a4=%ld a5=%ld",
                  tag, nr, a0, a1, a2, a3, a4, a5);
            break;
    }
}

static void apply_path_args(long nr, long *a0, long *a1, long *a2, long *a3, long *a4, long *a5) {
    (void) a4;
    (void) a5;
    switch (nr) {
        case __NR_openat:
        case __NR_openat2:
        case __NR_faccessat:
        case __NR_faccessat2:
        case __NR_fstatat:
        case __NR_unlinkat:
        case __NR_mkdirat:
        case __NR_readlinkat:
        case __NR_fchmodat:
        case __NR_mknodat:
        case __NR_statx:
        case __NR_fchownat:
            *a1 = reinterpret_cast<long>(
                    rewrite_path(reinterpret_cast<const char *>(*a1), g_path_a, sizeof(g_path_a)));
            break;
        case __NR_symlinkat:
            *a0 = reinterpret_cast<long>(
                    rewrite_path(reinterpret_cast<const char *>(*a0), g_path_a, sizeof(g_path_a)));
            *a2 = reinterpret_cast<long>(
                    rewrite_path(reinterpret_cast<const char *>(*a2), g_path_b, sizeof(g_path_b)));
            break;
        case __NR_linkat:
        case __NR_renameat:
        case __NR_renameat2:
            *a1 = reinterpret_cast<long>(
                    rewrite_path(reinterpret_cast<const char *>(*a1), g_path_a, sizeof(g_path_a)));
            *a3 = reinterpret_cast<long>(
                    rewrite_path(reinterpret_cast<const char *>(*a3), g_path_b, sizeof(g_path_b)));
            break;
        default:
            break;
    }
}

static void finish_trap(ucontext_t *uc, long ret) {
    // arm64 SVC exception already leaves PC at the next instruction.
    uc->uc_mcontext.regs[0] = static_cast<uint64_t>(ret);
}

static void seccomp_sigsys(int sig, siginfo_t *info, void *ucontext) {
    (void) sig;
    auto *uc = reinterpret_cast<ucontext_t *>(ucontext);
    if (info == nullptr || uc == nullptr) {
        return;
    }
    if (info->si_signo != SIGSYS && info->si_code != SYS_SECCOMP) {
        return;
    }

    long nr = static_cast<long>(uc->uc_mcontext.regs[8]);
    long a0 = static_cast<long>(uc->uc_mcontext.regs[0]);
    long a1 = static_cast<long>(uc->uc_mcontext.regs[1]);
    long a2 = static_cast<long>(uc->uc_mcontext.regs[2]);
    long a3 = static_cast<long>(uc->uc_mcontext.regs[3]);
    long a4 = static_cast<long>(uc->uc_mcontext.regs[4]);
    long a5 = static_cast<long>(uc->uc_mcontext.regs[5]);

    // Nested trap (e.g. logging): re-issue from safe page, no rewrite.
    if (g_in_handler || g_safe_syscall6 == nullptr || !is_path_syscall(nr)) {
        if (g_safe_syscall6 != nullptr) {
            finish_trap(uc, g_safe_syscall6(nr, a0, a1, a2, a3, a4, a5));
        } else {
            finish_trap(uc, -ENOSYS);
        }
        return;
    }

    g_in_handler++;
    log_svc_args(nr, a0, a1, a2, a3, a4, a5, true);
    apply_path_args(nr, &a0, &a1, &a2, &a3, &a4, &a5);
    log_svc_args(nr, a0, a1, a2, a3, a4, a5, false);
    long ret = g_safe_syscall6(nr, a0, a1, a2, a3, a4, a5);
    ALOGD("SeccompIo: ret %s = %ld", nr_name(nr), ret);
    finish_trap(uc, ret);
    g_in_handler--;
}

// Guest crash SDKs (XHS sentry) hook sigaction and reset SIGSYS to the
// default terminate action, which turns every trapped openat into a kill.
static int hooked_sigaction(int sig, const struct sigaction *act, struct sigaction *oldact) {
    if (g_lock_sigsys && sig == SIGSYS) {
        if (oldact != nullptr && orig_sigaction != nullptr) {
            orig_sigaction(SIGSYS, nullptr, oldact);
        }
        return 0;
    }
    return orig_sigaction(sig, act, oldact);
}

static int hooked_sigaction64(int sig, const void *act, void *oldact) {
    if (g_lock_sigsys && sig == SIGSYS) {
        if (oldact != nullptr && orig_sigaction64 != nullptr) {
            orig_sigaction64(SIGSYS, nullptr, oldact);
        }
        return 0;
    }
    return orig_sigaction64(sig, act, oldact);
}

static void lock_sigsys_handler() {
    void *handle = xdl_open("libc.so", XDL_DEFAULT);
    if (handle == nullptr) {
        ALOGE("SeccompIo: failed to open libc.so");
        return;
    }
    void *sigaction_sym = xdl_sym(handle, "sigaction", nullptr);
    void *sigaction64_sym = xdl_sym(handle, "sigaction64", nullptr);
    if (sigaction_sym != nullptr &&
        GumEngine::replace(sigaction_sym, (void *) hooked_sigaction, (void **) &orig_sigaction) == 0) {
        ALOGD("SeccompIo: locked sigaction(SIGSYS)");
    }
    if (sigaction64_sym != nullptr && sigaction64_sym != sigaction_sym &&
        GumEngine::replace(sigaction64_sym, (void *) hooked_sigaction64, (void **) &orig_sigaction64) == 0) {
        ALOGD("SeccompIo: locked sigaction64(SIGSYS)");
    }
    xdl_close(handle);
}

static bool install_sigsys_handler() {
    struct sigaction sa {};
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = seccomp_sigsys;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSYS, &sa, nullptr) != 0) {
        ALOGE("SeccompIo: sigaction(SIGSYS) failed: %s", strerror(errno));
        return false;
    }
    return true;
}

static bool install_filter() {
    if (!ensure_safe_page()) {
        return false;
    }

    const uint32_t page_lo = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_safe_page));
    const uint32_t page_hi = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_safe_page) >> 32);
    // Mask low 12 bits: any IP in this 4K page is our safe trampoline.
    // Compare (ip_hi == page_hi) && ((ip_lo & ~0xfff) == page_lo)

    static const uint32_t kPathNrs[] = {
            static_cast<uint32_t>(__NR_openat),
            static_cast<uint32_t>(__NR_openat2),
            static_cast<uint32_t>(__NR_faccessat),
            static_cast<uint32_t>(__NR_faccessat2),
            static_cast<uint32_t>(__NR_fstatat),
            static_cast<uint32_t>(__NR_statx),
            static_cast<uint32_t>(__NR_renameat),
            static_cast<uint32_t>(__NR_renameat2),
            static_cast<uint32_t>(__NR_unlinkat),
            static_cast<uint32_t>(__NR_mkdirat),
            static_cast<uint32_t>(__NR_readlinkat),
            static_cast<uint32_t>(__NR_fchmodat),
            static_cast<uint32_t>(__NR_fchownat),
            static_cast<uint32_t>(__NR_linkat),
            static_cast<uint32_t>(__NR_symlinkat),
            static_cast<uint32_t>(__NR_mknodat),
    };
    const size_t nr_count = sizeof(kPathNrs) / sizeof(kPathNrs[0]);

    sock_filter prog[96];
    size_t n = 0;

#define EMIT_STMT(_code, _k) do { \
        if (n >= sizeof(prog)/sizeof(prog[0])) return false; \
        prog[n++] = BPF_STMT((_code), (_k)); \
    } while (0)
#define EMIT_JUMP(_code, _jt, _jf, _k) do { \
        if (n >= sizeof(prog)/sizeof(prog[0])) return false; \
        prog[n++] = BPF_JUMP((_code), (_k), (_jt), (_jf)); \
    } while (0)

    // arch == AARCH64 ?
    EMIT_STMT(BPF_LD | BPF_W | BPF_ABS, (uint32_t) offsetof(seccomp_data, arch));
    EMIT_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 1, 0, SECCOMP_AUDIT_ARCH);
    EMIT_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);

    // if IP in safe page -> ALLOW
    // ip_hi
    EMIT_STMT(BPF_LD | BPF_W | BPF_ABS, (uint32_t) offsetof(seccomp_data, instruction_pointer) + 4);
    EMIT_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 3, page_hi); // miss -> skip allow (3 instr)
    EMIT_STMT(BPF_LD | BPF_W | BPF_ABS, (uint32_t) offsetof(seccomp_data, instruction_pointer));
    EMIT_STMT(BPF_ALU | BPF_AND | BPF_K, ~0xfffu);
    EMIT_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 1, page_lo); // miss -> fall through
    EMIT_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);

    // path NR -> TRAP
    EMIT_STMT(BPF_LD | BPF_W | BPF_ABS, (uint32_t) offsetof(seccomp_data, nr));
    for (size_t i = 0; i < nr_count; i++) {
        // On match: jump over remaining compares + ALLOW, land on TRAP.
        uint8_t to_trap = static_cast<uint8_t>((nr_count - i - 1) + 1);
        EMIT_JUMP(BPF_JMP | BPF_JEQ | BPF_K, to_trap, 0, kPathNrs[i]);
    }
    EMIT_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);
    EMIT_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP);

#undef EMIT_STMT
#undef EMIT_JUMP

    sock_fprog fprog {};
    fprog.len = static_cast<unsigned short>(n);
    fprog.filter = prog;

    prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);

    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &fprog) != 0) {
        ALOGE("SeccompIo: PR_SET_SECCOMP failed: %s", strerror(errno));
        return false;
    }
    return true;
}

bool SeccompIo_install() {
    if (g_installed) {
        return true;
    }
    lock_sigsys_handler();
    if (!install_sigsys_handler()) {
        return false;
    }
    g_lock_sigsys = true;
    if (!install_filter()) {
        g_lock_sigsys = false;
        return false;
    }
    g_installed = true;
    ALOGD("SeccompIo: path SVC trap installed, safe_page=%p", g_safe_page);
    return true;
}
