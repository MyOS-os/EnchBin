#pragma once
/* include/ench/syscall.h — диспетчер syscalls 0..26 (см. abi.h).
 * Вход через syscall/sysret (MSR LSTAR/STAR/SFMASK), единый C-диспетчер. */
#include <stdint.h>

struct syscall_frame {
    uint64_t rdi, rsi, rdx, r10, r8, r9;
    int64_t  n;      /* rax */
};

void syscall_init(void);
int64_t syscall_dispatch(struct syscall_frame *f);

/* syscall-обёртка ядра (для oss_trace/filelog и тестов) */
int64_t ench_syscall(int64_t n, int64_t a1, int64_t a2, int64_t a3,
                     int64_t a4, int64_t a5, int64_t a6);
