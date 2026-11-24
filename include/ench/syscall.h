/* include/ench/syscall.h — диспетчер syscalls 0..26 (документация).
 * Реализация: kernel/src/syscall/syscall.c + вход syscall_entry в
 * kernel/src/cpu/cpu.c (SYSCALL/SYSRET: LSTAR/STAR/FMASK, MSR).
 * Контракт: номер в rax, SysV-аргументы rdi,rsi,rdx,r10,r8,r9; ошибка =>
 * отрицательный errno (ENCH_E*); кадр struct cpu_frame (ench/kcpu.h),
 * sys_dispatch возвращает кадр для iretq (exit/yield/sleep/wait могут
 * переключить задачу). Таблица вызовов — SYS_* в ench/abi.h,
 * ring3-обёртки — ench/user.h (ench_sys0..6, syscall-инструкция). */
#ifndef ENCH_SYSCALL_H
#define ENCH_SYSCALL_H
#include <ench/abi.h>
#endif
