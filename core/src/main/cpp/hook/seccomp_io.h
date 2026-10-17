#ifndef BLACKBOX_SECCOMP_IO_H
#define BLACKBOX_SECCOMP_IO_H

// Trap raw SVC for path-bearing syscalls and rewrite via IO::redirectPath.
// Re-entry uses a small safe syscall trampoline allowlisted by instruction pointer.
bool SeccompIo_install();

#endif
