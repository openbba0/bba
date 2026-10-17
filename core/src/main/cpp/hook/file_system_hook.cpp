#include "file_system_hook.h"
#include "log.h"
#include "xdl.h"
#include "io.h"
#include "hook/gum_engine.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <unistd.h>

#ifndef O_TMPFILE
#define O_TMPFILE 020000000
#endif

// Opaque enough for trampoline; we only rewrite the path argument.
using openat2_fn = int (*)(int, const char *, void *, size_t);

static int (*orig_openat)(int dirfd, const char *pathname, int flags, ...) = nullptr;
static int (*orig_open)(const char *pathname, int flags, ...) = nullptr;
static int (*orig_open64)(const char *pathname, int flags, ...) = nullptr;
static int (*orig_open_2)(const char *pathname, int flags) = nullptr;
static openat2_fn orig_openat2 = nullptr;
static int (*orig_faccessat)(int dirfd, const char *pathname, int mode, int flags) = nullptr;
static int (*orig_faccessat2)(int dirfd, const char *pathname, int mode, int flags) = nullptr;
static int (*orig_fstatat)(int dirfd, const char *pathname, struct stat *buf, int flags) = nullptr;
static int (*orig_statx)(int dirfd, const char *pathname, int flags, unsigned mask, void *buf) = nullptr;
static int (*orig_renameat)(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) = nullptr;
static int (*orig_renameat2)(int olddirfd, const char *oldpath, int newdirfd, const char *newpath,
                             unsigned flags) = nullptr;
static int (*orig_unlinkat)(int dirfd, const char *pathname, int flags) = nullptr;
static int (*orig_mkdirat)(int dirfd, const char *pathname, mode_t mode) = nullptr;
static ssize_t (*orig_readlinkat)(int dirfd, const char *pathname, char *buf, size_t bufsiz) = nullptr;
static int (*orig_fchmodat)(int dirfd, const char *pathname, mode_t mode, int flags) = nullptr;
static int (*orig_fchownat)(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) = nullptr;
static int (*orig_linkat)(int olddirfd, const char *oldpath, int newdirfd, const char *newpath,
                          int flags) = nullptr;
static int (*orig_symlinkat)(const char *target, int newdirfd, const char *linkpath) = nullptr;
static int (*orig_mknodat)(int dirfd, const char *pathname, mode_t mode, dev_t dev) = nullptr;

// Non-*at wrappers still common in older / third-party code.
static int (*orig_access)(const char *pathname, int mode) = nullptr;
static int (*orig_stat)(const char *pathname, struct stat *buf) = nullptr;
static int (*orig_lstat)(const char *pathname, struct stat *buf) = nullptr;
static int (*orig_rename)(const char *oldpath, const char *newpath) = nullptr;
static int (*orig_unlink)(const char *pathname) = nullptr;
static int (*orig_rmdir)(const char *pathname) = nullptr;
static int (*orig_mkdir)(const char *pathname, mode_t mode) = nullptr;
static ssize_t (*orig_readlink)(const char *pathname, char *buf, size_t bufsiz) = nullptr;
static int (*orig_chmod)(const char *pathname, mode_t mode) = nullptr;
static int (*orig_chown)(const char *pathname, uid_t owner, gid_t group) = nullptr;
static int (*orig_lchown)(const char *pathname, uid_t owner, gid_t group) = nullptr;
static int (*orig_link)(const char *oldpath, const char *newpath) = nullptr;
static int (*orig_symlink)(const char *target, const char *linkpath) = nullptr;
static int (*orig_mknod)(const char *pathname, mode_t mode, dev_t dev) = nullptr;

static thread_local int g_fs_hook_depth = 0;

static const char *redirect_path(const char *pathname) {
    if (pathname == nullptr) {
        return pathname;
    }
    return IO::redirectPath(pathname);
}

static bool needs_mode(int flags) {
    return (flags & O_CREAT) != 0 || (flags & O_TMPFILE) == O_TMPFILE;
}

#define FS_GUARD_BEGIN() if (g_fs_hook_depth > 0)
#define FS_CALL_BEGIN() g_fs_hook_depth++
#define FS_CALL_END() g_fs_hook_depth--

static int hooked_openat(int dirfd, const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (needs_mode(flags)) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }

    FS_GUARD_BEGIN() {
        if (needs_mode(flags)) {
            return orig_openat(dirfd, pathname, flags, mode);
        }
        return orig_openat(dirfd, pathname, flags);
    }

    FS_CALL_BEGIN();
    const char *redirected = redirect_path(pathname);
    int rc;
    if (needs_mode(flags)) {
        rc = orig_openat(dirfd, redirected, flags, mode);
    } else {
        rc = orig_openat(dirfd, redirected, flags);
    }
    FS_CALL_END();
    return rc;
}

static int hooked_open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (needs_mode(flags)) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }

    FS_GUARD_BEGIN() {
        if (needs_mode(flags)) {
            return orig_open(pathname, flags, mode);
        }
        return orig_open(pathname, flags);
    }

    FS_CALL_BEGIN();
    const char *redirected = redirect_path(pathname);
    int rc;
    if (needs_mode(flags)) {
        rc = orig_open(redirected, flags, mode);
    } else {
        rc = orig_open(redirected, flags);
    }
    FS_CALL_END();
    return rc;
}

static int hooked_open64(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (needs_mode(flags)) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }

    FS_GUARD_BEGIN() {
        if (needs_mode(flags)) {
            return orig_open64(pathname, flags, mode);
        }
        return orig_open64(pathname, flags);
    }

    FS_CALL_BEGIN();
    const char *redirected = redirect_path(pathname);
    int rc;
    if (needs_mode(flags)) {
        rc = orig_open64(redirected, flags, mode);
    } else {
        rc = orig_open64(redirected, flags);
    }
    FS_CALL_END();
    return rc;
}

static int hooked_open_2(const char *pathname, int flags) {
    FS_GUARD_BEGIN() {
        return orig_open_2(pathname, flags);
    }
    FS_CALL_BEGIN();
    int rc = orig_open_2(redirect_path(pathname), flags);
    FS_CALL_END();
    return rc;
}

static int hooked_openat2(int dirfd, const char *pathname, void *how, size_t size) {
    FS_GUARD_BEGIN() {
        return orig_openat2(dirfd, pathname, how, size);
    }
    FS_CALL_BEGIN();
    int rc = orig_openat2(dirfd, redirect_path(pathname), how, size);
    FS_CALL_END();
    return rc;
}

static int hooked_faccessat(int dirfd, const char *pathname, int mode, int flags) {
    FS_GUARD_BEGIN() {
        return orig_faccessat(dirfd, pathname, mode, flags);
    }
    FS_CALL_BEGIN();
    int rc = orig_faccessat(dirfd, redirect_path(pathname), mode, flags);
    FS_CALL_END();
    return rc;
}

static int hooked_faccessat2(int dirfd, const char *pathname, int mode, int flags) {
    FS_GUARD_BEGIN() {
        return orig_faccessat2(dirfd, pathname, mode, flags);
    }
    FS_CALL_BEGIN();
    int rc = orig_faccessat2(dirfd, redirect_path(pathname), mode, flags);
    FS_CALL_END();
    return rc;
}

static int hooked_fstatat(int dirfd, const char *pathname, struct stat *buf, int flags) {
    FS_GUARD_BEGIN() {
        return orig_fstatat(dirfd, pathname, buf, flags);
    }
    FS_CALL_BEGIN();
    int rc = orig_fstatat(dirfd, redirect_path(pathname), buf, flags);
    FS_CALL_END();
    return rc;
}

static int hooked_statx(int dirfd, const char *pathname, int flags, unsigned mask, void *buf) {
    FS_GUARD_BEGIN() {
        return orig_statx(dirfd, pathname, flags, mask, buf);
    }
    FS_CALL_BEGIN();
    int rc = orig_statx(dirfd, redirect_path(pathname), flags, mask, buf);
    FS_CALL_END();
    return rc;
}

static int hooked_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
    FS_GUARD_BEGIN() {
        return orig_renameat(olddirfd, oldpath, newdirfd, newpath);
    }
    FS_CALL_BEGIN();
    int rc = orig_renameat(olddirfd, redirect_path(oldpath), newdirfd, redirect_path(newpath));
    FS_CALL_END();
    return rc;
}

static int hooked_renameat2(int olddirfd, const char *oldpath, int newdirfd, const char *newpath,
                            unsigned flags) {
    FS_GUARD_BEGIN() {
        return orig_renameat2(olddirfd, oldpath, newdirfd, newpath, flags);
    }
    FS_CALL_BEGIN();
    int rc = orig_renameat2(olddirfd, redirect_path(oldpath), newdirfd, redirect_path(newpath), flags);
    FS_CALL_END();
    return rc;
}

static int hooked_unlinkat(int dirfd, const char *pathname, int flags) {
    FS_GUARD_BEGIN() {
        return orig_unlinkat(dirfd, pathname, flags);
    }
    FS_CALL_BEGIN();
    int rc = orig_unlinkat(dirfd, redirect_path(pathname), flags);
    FS_CALL_END();
    return rc;
}

static int hooked_mkdirat(int dirfd, const char *pathname, mode_t mode) {
    FS_GUARD_BEGIN() {
        return orig_mkdirat(dirfd, pathname, mode);
    }
    FS_CALL_BEGIN();
    int rc = orig_mkdirat(dirfd, redirect_path(pathname), mode);
    FS_CALL_END();
    return rc;
}

static ssize_t hooked_readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz) {
    FS_GUARD_BEGIN() {
        return orig_readlinkat(dirfd, pathname, buf, bufsiz);
    }
    FS_CALL_BEGIN();
    ssize_t rc = orig_readlinkat(dirfd, redirect_path(pathname), buf, bufsiz);
    FS_CALL_END();
    return rc;
}

static int hooked_fchmodat(int dirfd, const char *pathname, mode_t mode, int flags) {
    FS_GUARD_BEGIN() {
        return orig_fchmodat(dirfd, pathname, mode, flags);
    }
    FS_CALL_BEGIN();
    int rc = orig_fchmodat(dirfd, redirect_path(pathname), mode, flags);
    FS_CALL_END();
    return rc;
}

static int hooked_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags) {
    FS_GUARD_BEGIN() {
        return orig_fchownat(dirfd, pathname, owner, group, flags);
    }
    FS_CALL_BEGIN();
    int rc = orig_fchownat(dirfd, redirect_path(pathname), owner, group, flags);
    FS_CALL_END();
    return rc;
}

static int hooked_linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags) {
    FS_GUARD_BEGIN() {
        return orig_linkat(olddirfd, oldpath, newdirfd, newpath, flags);
    }
    FS_CALL_BEGIN();
    int rc = orig_linkat(olddirfd, redirect_path(oldpath), newdirfd, redirect_path(newpath), flags);
    FS_CALL_END();
    return rc;
}

static int hooked_symlinkat(const char *target, int newdirfd, const char *linkpath) {
    FS_GUARD_BEGIN() {
        return orig_symlinkat(target, newdirfd, linkpath);
    }
    FS_CALL_BEGIN();
    // Symlink body is content, not a filesystem open path; only rewrite the link location.
    int rc = orig_symlinkat(target, newdirfd, redirect_path(linkpath));
    FS_CALL_END();
    return rc;
}

static int hooked_mknodat(int dirfd, const char *pathname, mode_t mode, dev_t dev) {
    FS_GUARD_BEGIN() {
        return orig_mknodat(dirfd, pathname, mode, dev);
    }
    FS_CALL_BEGIN();
    int rc = orig_mknodat(dirfd, redirect_path(pathname), mode, dev);
    FS_CALL_END();
    return rc;
}

static int hooked_access(const char *pathname, int mode) {
    FS_GUARD_BEGIN() {
        return orig_access(pathname, mode);
    }
    FS_CALL_BEGIN();
    int rc = orig_access(redirect_path(pathname), mode);
    FS_CALL_END();
    return rc;
}

static int hooked_stat(const char *pathname, struct stat *buf) {
    FS_GUARD_BEGIN() {
        return orig_stat(pathname, buf);
    }
    FS_CALL_BEGIN();
    int rc = orig_stat(redirect_path(pathname), buf);
    FS_CALL_END();
    return rc;
}

static int hooked_lstat(const char *pathname, struct stat *buf) {
    FS_GUARD_BEGIN() {
        return orig_lstat(pathname, buf);
    }
    FS_CALL_BEGIN();
    int rc = orig_lstat(redirect_path(pathname), buf);
    FS_CALL_END();
    return rc;
}

static int hooked_rename(const char *oldpath, const char *newpath) {
    FS_GUARD_BEGIN() {
        return orig_rename(oldpath, newpath);
    }
    FS_CALL_BEGIN();
    int rc = orig_rename(redirect_path(oldpath), redirect_path(newpath));
    FS_CALL_END();
    return rc;
}

static int hooked_unlink(const char *pathname) {
    FS_GUARD_BEGIN() {
        return orig_unlink(pathname);
    }
    FS_CALL_BEGIN();
    int rc = orig_unlink(redirect_path(pathname));
    FS_CALL_END();
    return rc;
}

static int hooked_rmdir(const char *pathname) {
    FS_GUARD_BEGIN() {
        return orig_rmdir(pathname);
    }
    FS_CALL_BEGIN();
    int rc = orig_rmdir(redirect_path(pathname));
    FS_CALL_END();
    return rc;
}

static int hooked_mkdir(const char *pathname, mode_t mode) {
    FS_GUARD_BEGIN() {
        return orig_mkdir(pathname, mode);
    }
    FS_CALL_BEGIN();
    int rc = orig_mkdir(redirect_path(pathname), mode);
    FS_CALL_END();
    return rc;
}

static ssize_t hooked_readlink(const char *pathname, char *buf, size_t bufsiz) {
    FS_GUARD_BEGIN() {
        return orig_readlink(pathname, buf, bufsiz);
    }
    FS_CALL_BEGIN();
    ssize_t rc = orig_readlink(redirect_path(pathname), buf, bufsiz);
    FS_CALL_END();
    return rc;
}

static int hooked_chmod(const char *pathname, mode_t mode) {
    FS_GUARD_BEGIN() {
        return orig_chmod(pathname, mode);
    }
    FS_CALL_BEGIN();
    int rc = orig_chmod(redirect_path(pathname), mode);
    FS_CALL_END();
    return rc;
}

static int hooked_chown(const char *pathname, uid_t owner, gid_t group) {
    FS_GUARD_BEGIN() {
        return orig_chown(pathname, owner, group);
    }
    FS_CALL_BEGIN();
    int rc = orig_chown(redirect_path(pathname), owner, group);
    FS_CALL_END();
    return rc;
}

static int hooked_lchown(const char *pathname, uid_t owner, gid_t group) {
    FS_GUARD_BEGIN() {
        return orig_lchown(pathname, owner, group);
    }
    FS_CALL_BEGIN();
    int rc = orig_lchown(redirect_path(pathname), owner, group);
    FS_CALL_END();
    return rc;
}

static int hooked_link(const char *oldpath, const char *newpath) {
    FS_GUARD_BEGIN() {
        return orig_link(oldpath, newpath);
    }
    FS_CALL_BEGIN();
    int rc = orig_link(redirect_path(oldpath), redirect_path(newpath));
    FS_CALL_END();
    return rc;
}

static int hooked_symlink(const char *target, const char *linkpath) {
    FS_GUARD_BEGIN() {
        return orig_symlink(target, linkpath);
    }
    FS_CALL_BEGIN();
    int rc = orig_symlink(target, redirect_path(linkpath));
    FS_CALL_END();
    return rc;
}

static int hooked_mknod(const char *pathname, mode_t mode, dev_t dev) {
    FS_GUARD_BEGIN() {
        return orig_mknod(pathname, mode, dev);
    }
    FS_CALL_BEGIN();
    int rc = orig_mknod(redirect_path(pathname), mode, dev);
    FS_CALL_END();
    return rc;
}

static void replace_sym(void *handle, const char *name, void *replacement, void **original) {
    void *target = xdl_sym(handle, name, nullptr);
    if (target == nullptr) {
        ALOGD("FileSystemHook: symbol %s not found", name);
        return;
    }
    if (GumEngine::replace(target, replacement, original) == 0) {
        ALOGD("FileSystemHook: hooked %s at %p", name, target);
    } else {
        ALOGE("FileSystemHook: failed to hook %s at %p", name, target);
    }
}

static void replace_sym_alias(void *handle, const char *name, void *replacement, void **original,
                              void *alias_of) {
    void *target = xdl_sym(handle, name, nullptr);
    if (target == nullptr) {
        ALOGD("FileSystemHook: symbol %s not found", name);
        return;
    }
    if (alias_of != nullptr && target == alias_of && *original != nullptr) {
        ALOGD("FileSystemHook: %s aliases existing hook, reuse trampoline", name);
        return;
    }
    if (GumEngine::replace(target, replacement, original) == 0) {
        ALOGD("FileSystemHook: hooked %s at %p", name, target);
    } else {
        ALOGE("FileSystemHook: failed to hook %s at %p", name, target);
    }
}

void FileSystemHook::init() {
    ALOGD("FileSystemHook: installing libc path hooks");

    void *handle = xdl_open("libc.so", XDL_DEFAULT);
    if (!handle) {
        ALOGE("FileSystemHook: failed to open libc.so");
        return;
    }

    void *open_addr = xdl_sym(handle, "open", nullptr);
    void *open64_addr = xdl_sym(handle, "open64", nullptr);

    replace_sym(handle, "openat", (void *) hooked_openat, (void **) &orig_openat);
    replace_sym(handle, "open", (void *) hooked_open, (void **) &orig_open);
    // open64 is often an alias of open; Gum cannot replace the same address twice.
    if (open64_addr != nullptr && open64_addr == open_addr && orig_open != nullptr) {
        orig_open64 = orig_open;
        ALOGD("FileSystemHook: open64 aliases open, reuse trampoline");
    } else {
        replace_sym(handle, "open64", (void *) hooked_open64, (void **) &orig_open64);
    }
    replace_sym(handle, "__open_2", (void *) hooked_open_2, (void **) &orig_open_2);
    replace_sym(handle, "openat2", (void *) hooked_openat2, (void **) &orig_openat2);

    replace_sym(handle, "faccessat", (void *) hooked_faccessat, (void **) &orig_faccessat);
    replace_sym(handle, "faccessat2", (void *) hooked_faccessat2, (void **) &orig_faccessat2);
    replace_sym(handle, "fstatat", (void *) hooked_fstatat, (void **) &orig_fstatat);
    if (orig_fstatat == nullptr) {
        replace_sym(handle, "fstatat64", (void *) hooked_fstatat, (void **) &orig_fstatat);
    }
    // newfstatat may share address with fstatat on aarch64.
    replace_sym_alias(handle, "newfstatat", (void *) hooked_fstatat, (void **) &orig_fstatat,
                      xdl_sym(handle, "fstatat", nullptr));
    replace_sym(handle, "statx", (void *) hooked_statx, (void **) &orig_statx);

    replace_sym(handle, "renameat", (void *) hooked_renameat, (void **) &orig_renameat);
    replace_sym(handle, "renameat2", (void *) hooked_renameat2, (void **) &orig_renameat2);
    replace_sym(handle, "unlinkat", (void *) hooked_unlinkat, (void **) &orig_unlinkat);
    replace_sym(handle, "mkdirat", (void *) hooked_mkdirat, (void **) &orig_mkdirat);
    replace_sym(handle, "readlinkat", (void *) hooked_readlinkat, (void **) &orig_readlinkat);
    replace_sym(handle, "fchmodat", (void *) hooked_fchmodat, (void **) &orig_fchmodat);
    replace_sym(handle, "fchownat", (void *) hooked_fchownat, (void **) &orig_fchownat);
    replace_sym(handle, "linkat", (void *) hooked_linkat, (void **) &orig_linkat);
    replace_sym(handle, "symlinkat", (void *) hooked_symlinkat, (void **) &orig_symlinkat);
    replace_sym(handle, "mknodat", (void *) hooked_mknodat, (void **) &orig_mknodat);

    replace_sym(handle, "access", (void *) hooked_access, (void **) &orig_access);
    replace_sym(handle, "stat", (void *) hooked_stat, (void **) &orig_stat);
    replace_sym(handle, "lstat", (void *) hooked_lstat, (void **) &orig_lstat);
    replace_sym(handle, "rename", (void *) hooked_rename, (void **) &orig_rename);
    replace_sym(handle, "unlink", (void *) hooked_unlink, (void **) &orig_unlink);
    replace_sym(handle, "rmdir", (void *) hooked_rmdir, (void **) &orig_rmdir);
    replace_sym(handle, "mkdir", (void *) hooked_mkdir, (void **) &orig_mkdir);
    replace_sym(handle, "readlink", (void *) hooked_readlink, (void **) &orig_readlink);
    replace_sym(handle, "chmod", (void *) hooked_chmod, (void **) &orig_chmod);
    replace_sym(handle, "chown", (void *) hooked_chown, (void **) &orig_chown);
    replace_sym(handle, "lchown", (void *) hooked_lchown, (void **) &orig_lchown);
    replace_sym(handle, "link", (void *) hooked_link, (void **) &orig_link);
    replace_sym(handle, "symlink", (void *) hooked_symlink, (void **) &orig_symlink);
    replace_sym(handle, "mknod", (void *) hooked_mknod, (void **) &orig_mknod);

    xdl_close(handle);
}
