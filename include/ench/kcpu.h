// include/ench/kcpu.h — внутренние структуры CPU и хуки ядра/планировщика.
// Используется ТОЛЬКО ring0-кодом (kernel/src/*): cpu.c, task.c, syscall.c.
#pragma once

#include <ench/base.h>

// Кадр прерывания/syscall: единая раскладка (ISR-стаб и syscall_entry
// строят её одинаково: r15..rax, vector, err, rip, cs, rflags, rsp, ss).
struct cpu_frame {
    u64 r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 vector, err;
    u64 rip, cs, rflags, rsp, ss;
};

// ---- cpu/cpu.c ----
extern u64 uptime_ticks;              // тики PIT = миллисекунды (PIT 1000 Гц)
void cpu_set_ksp(u64 ksp);            // стек syscall-входа (текущая задача)
void cpu_set_tss_rsp0(u64 rsp);
void cpu_set_kbd_hook(void (*fn)(u8 sc)); // хук сканкодов (драйвер keyboard.b_d)
// построить начальный кадр на kernel-стеке: rip/rsp/сегменты; user=1 -> ring3
void *cpu_make_frame(u64 kstack_top, u64 rip, u64 rsp, u8 user);
// восстановить кадр и войти в задачу (pop r15..rax, add $16, iretq)
void cpu_restore_frame(struct cpu_frame *fr) __attribute__((noreturn));

// ---- task/task.c (сон/ожидание через SYS_sleep/SYS_wait) ----
void task_sleep_begin(u64 ms);
int  task_sleep_pending(void);
void task_sleep_end(void);
i32  task_wait_check(i32 pid, i32 *code_out); /* 1=DEAD(код готов), 0=работает, <0=err */
i32  task_current_pid(void);
void task_set_block(void);
void task_set_run(void);

// ---- task/task.c (переключение из рамки) ----
void *sched_timer(struct cpu_frame *fr);  // тик PIT: вернуть кадр новой задачи или 0
void *task_exit_switch(void);             // после task_exit: кадр следующей задачи
void *task_yield_switch(void *fr);        // SYS_yield: немедленная передача CPU
u32   task_weight_for_size(usize n);      // вес = размер образа/1024, [1..1024]
u64   task_getcur_brk(void);
void  task_getcur_brk_set(u64 nb);

// ---- syscall/syscall.c ----
void *sys_dispatch(struct cpu_frame *fr);
