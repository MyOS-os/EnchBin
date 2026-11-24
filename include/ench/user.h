// include/ench/user.h — ring3 API EnchBin (реализация в libench, stdlib/c).
// Все операции идут через syscalls 0..26 (см. ench/abi.h).
#pragma once

#include <ench/base.h>
#include <ench/abi.h>
#include <ench/osstrace.h>

// ---- raw syscall ----
i64 ench_sys0(u64 n);
i64 ench_sys1(u64 n, u64 a);
i64 ench_sys2(u64 n, u64 a, u64 b);
i64 ench_sys3(u64 n, u64 a, u64 b, u64 c);
i64 ench_sys4(u64 n, u64 a, u64 b, u64 c, u64 d);
i64 ench_sys5(u64 n, u64 a, u64 b, u64 c, u64 d, u64 e);
i64 ench_sys6(u64 n, u64 a, u64 b, u64 c, u64 d, u64 e, u64 f);

// ---- crt0 / процесс ----
void _exit(i32 code) __attribute__((noreturn)); /* ядро печатает J4-строку exit N */

// ---- консоль: программа владеет выводом; при выходе ядро печатает "exit N" ----
int printf(const char *fmt, ...);
int puts(const char *s);
int putchar(int c);
// чтение строки со stdin; echo=0 -> ввод пароля (звёздочки)
char *getline(char *buf, int cap, int echo);

// ---- fs ----
i32 e_open(const char *path, int mode);
i32 e_read(i32 fd, void *buf, usize n);
i32 e_write(i32 fd, const void *buf, usize n);
void e_close(i32 fd);
i32 e_stat(const char *path, struct ench_fstat *st);
i32 e_mkdir(const char *path);
i32 e_unlink(const char *path);
i32 e_listdir(const char *path, char *out, usize cap, int show_hidden);
i32 e_chdir(const char *path);
usize e_getcwd(char *out, usize cap);

// ---- планировщик: вес + реальный поток ----
void ench_setweight(u32 w);              /* SYS_setweight */
u32 ench_weight_for_size(usize bytes);   /* вес = размер кода /1024, [1..1024] */
void ench_setrt(int on);                 /* SYS_setrt: не прерывать по тику */
void ench_yield(void);
void ench_sleep(u64 ms);
i32 ench_spawn(const char *path, const char *const *argv);
i32 ench_wait(i32 pid, i32 *code_out);
i32 ench_tasklist(struct ench_taskinfo *out, i32 max);
i32 ench_getpid(void);
void ench_kill(i32 pid);

// ---- динамический ассемблер ----
i32 ench_asmlist(char *out, usize cap);           /* файлы *.easm */
i32 ench_asmrun(const char *name, i64 *retval);   /* запуск /Platform/data/asm/<name> */

// ---- drvctl ----
i32 ench_drvctl_list(char *out, usize cap);
i32 ench_drvctl_load(const char *path);
i32 ench_drvctl_call(const char *name, u32 cmd, u64 arg);

// ---- аккаунт ----
i32 ench_acc_create(const char *login, const char *pass);
i32 ench_acc_login(const char *login, const char *pass);
i32 ench_whoami(char *out, usize cap);

// ---- прочее ----
void ench_time(struct ench_rtc *out);
i32 ench_powerctl(int op); /* 1=reboot 2=halt */
void *ench_sbrk(isize delta);
