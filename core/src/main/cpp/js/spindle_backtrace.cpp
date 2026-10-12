#include "spindle_backtrace.h"

#include <string.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>
#include <unwind.h>

#include <vector>

struct ExecRange {
    uint64_t start;
    uint64_t end;
};

static std::vector<ExecRange> load_exec_ranges() {
    std::vector<ExecRange> ranges;
    FILE *fp = fopen("/proc/self/maps", "re");
    char line[512];
    if (fp == nullptr) {
        return ranges;
    }
    while (fgets(line, sizeof(line), fp) != nullptr) {
        unsigned long start = 0;
        unsigned long end = 0;
        char perms[8];
        if (sscanf(line, "%lx-%lx %7s", &start, &end, perms) != 3) {
            continue;
        }
        if (perms[2] == 'x') {
            ranges.push_back(ExecRange{start, end});
        }
    }
    fclose(fp);
    return ranges;
}

static int in_exec(const std::vector<ExecRange> &ranges, uint64_t pc) {
    if ((pc & 3) != 0 || pc < 0x1000) {
        return 0;
    }
    for (const ExecRange &range : ranges) {
        if (pc >= range.start && pc < range.end) {
            return 1;
        }
    }
    return 0;
}

static int read_u64(uint64_t addr, uint64_t *out) {
    struct iovec local;
    struct iovec remote;
    local.iov_base = out;
    local.iov_len = sizeof(*out);
    remote.iov_base = reinterpret_cast<void *>(addr);
    remote.iov_len = sizeof(*out);
    return syscall(__NR_process_vm_readv, getpid(), &local, 1, &remote, 1, 0) ==
           (ssize_t) sizeof(*out);
}

static int push_pc(uint64_t *pcs, int n, int max, uint64_t pc) {
    if (pc < 0x1000 || n >= max) {
        return n;
    }
    if (n > 0 && pcs[n - 1] == pc) {
        return n;
    }
    pcs[n] = pc;
    return n + 1;
}

static int backtrace_fuzzy(const struct spindle_cpu_snap *snap, uint64_t *pcs, int max) {
    struct spindle_cpu_snap local;
    if (snap == nullptr) {
        uint64_t sp = 0;
        uint64_t lr = 0;
        memset(&local, 0, sizeof(local));
        asm volatile("mov %0, sp" : "=r"(sp));
        asm volatile("mov %0, x30" : "=r"(lr));
        local.pc = reinterpret_cast<uint64_t>(__builtin_return_address(0));
        local.sp = sp;
        local.lr = lr;
        snap = &local;
    }
    std::vector<ExecRange> ranges = load_exec_ranges();
    int n = push_pc(pcs, 0, max, snap->pc);
    if (in_exec(ranges, snap->lr)) {
        n = push_pc(pcs, n, max, snap->lr);
    }
    uint64_t cursor = snap->sp;
    int words = 0;
    while (n < max && words < 2048) {
        uint64_t word = 0;
        if ((cursor & 7) != 0 || !read_u64(cursor, &word)) {
            break;
        }
        if (in_exec(ranges, word)) {
            n = push_pc(pcs, n, max, word);
        }
        cursor += sizeof(word);
        words++;
    }
    return n;
}

struct UnwindState {
    uint64_t *pcs;
    int n;
    int max;
};

static _Unwind_Reason_Code unwind_frame(struct _Unwind_Context *ctx, void *arg) {
    auto *state = static_cast<UnwindState *>(arg);
    if (state->n >= state->max) {
        return _URC_END_OF_STACK;
    }
    state->pcs[state->n++] = static_cast<uint64_t>(_Unwind_GetIP(ctx));
    return _URC_NO_REASON;
}

extern "C" int spindle_backtrace(const struct spindle_cpu_snap *snap, int fuzzy, uint64_t *pcs,
                                 int max) {
    if (pcs == nullptr || max <= 0) {
        return 0;
    }
    if (max > SPINDLE_BT_MAX) {
        max = SPINDLE_BT_MAX;
    }
    if (fuzzy || snap != nullptr) {
        return backtrace_fuzzy(snap, pcs, max);
    }
    UnwindState state{pcs, 0, max};
    _Unwind_Backtrace(unwind_frame, &state);
    return state.n;
}
