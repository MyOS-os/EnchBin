// include/ench/kernel.h — внутренний API ядра EnchBin (ring 0).
#pragma once

#include <ench/base.h>
#include <ench/abi.h>
#include <ench/limine.h>

extern const char ench_version[];

// ---- boot (boot/main.c) ----
struct limine_framebuffer *ench_boot_fb(void);
u64 ench_boot_hhdm(void);
struct limine_memmap_response *ench_boot_memmap(void);
struct limine_file *ench_boot_module(void);

// ---- console (console/console.c) — протокол J4 ----
enum {
    ENCH_BLACK, ENCH_BLUE, ENCH_GREEN, ENCH_CYAN, ENCH_RED, ENCH_MAGENTA,
    ENCH_BROWN, ENCH_LGRAY, ENCH_DGRAY, ENCH_LBLUE, ENCH_LGREEN, ENCH_LCYAN,
    ENCH_LRED, ENCH_LMAGENTA, ENCH_YELLOW, ENCH_WHITE
};
void kconsole_init(void);
void kputc(char c);
void kputs(const char *s);
void kprintf(const char *fmt, ...);
void kclear(void);
void ksetattr(u8 fg, u8 bg);
void kscroll(int n);
void kfont_glyph(char ch, u8 rows[5]);   /* 3-битные строки глифа 3x5 */
// J4-сессии: owner == NULL -> режим kernel-лога; пока owner != NULL,
// kprintf уходит на COM1/filelog, а владелец рисует на экране.
void console_acquire(void *owner);
void console_release(void *owner);
// строка приложения поверх J4 (всегда на экран, независимо от owner)
void console_app_write(const char *s, usize n);
// J4-строка выхода: последняя строка, отдельно на новой строке
void console_exit_line(i32 code);
// очередь ввода (заполняет драйвер keyboard.b_d через kinput_push)
void kinput_push(char c);
int  kinput_pop(char *out, int block);   /* block=1 -> ждать (hlt) */
void kserial_init(void);
void kserial_write(const char *s, usize n);

// ---- форматтер ядра (console/fmt.c) ----
int kvformat(char *out, usize cap, const char *fmt, va_list ap);

// ---- mm (mm/mm.c) ----
void pmm_init(struct limine_memmap_response *mm);
void *pmm_alloc(void);
void pmm_free(void *page);
usize pmm_free_kb(void);
usize pmm_total_kb(void);
void vmm_init(u64 hhdm_off);
void vmm_map(u64 vaddr, u64 paddr, u64 flags); /* flags: bit0=W, bit1=U, bit2=NX off */
void *kmalloc(usize n);
void kfree(void *p);
void *krealloc(void *p, usize n);

// ---- task (task/task.c): weighted round-robin + rt-потоки ----
// Вес по умолчанию = размер образа (ELF) /1024, clamped [1,1024];
// SYS_setweight перекрывает. Квант = базовый * вес.
// rt-поток: планировщик не вытесняет его по тику, пока он сам не заблокируется.
void task_init(void);
i32 task_spawn_elf(const char *path, const char *const *argv, i32 *pid_out);
i32 task_spawn_kth(void (*fn)(void *), void *arg, const char *name);
void task_yield(void);
void task_exit(i32 code) __attribute__((noreturn));
void task_exit_core(i32 code);   /* только бухучёт: для SYS_exit (sys_dispatch вернёт кадр сам) */
void task_reap(i32 pid);         /* освободить стек zombie-задачи после wait */
void task_setweight(i32 pid, u32 w);
void task_setrt(i32 pid, int on);
void task_kill(i32 pid);
i32 task_list(struct ench_taskinfo *out, i32 max);
void scheduler_tick(void);
void task_enter_idle(void) __attribute__((noreturn));

// ---- fs (fs/fs.c): ramfs поверх ustar-модуля + write-оверлей + lock-файлы ----
// Правила имён: имя с '.' в начале или суффиксом ".__" скрыто (ls --a);
// суффикс ".__" => lock: чтение -> ENCH_ELOCKED, запись/удаление -> ENCH_EACCES.
i32 fs_open(const char *path, int mode);
i32 fs_read(i32 fd, void *buf, usize n);
i32 fs_write(i32 fd, const void *buf, usize n);
void fs_close(i32 fd);
i32 fs_stat(const char *path, struct ench_fstat *st);
i32 fs_mkdir(const char *path);
i32 fs_unlink(const char *path);
// listdir: пишет "имя\n"; show_hidden=0 прячет точки и .__
i32 fs_listdir(const char *path, char *out, usize cap, int show_hidden);
i32 fs_chdir(const char *path);
usize fs_getcwd(char *out, usize cap);
i32 fs_log_write(const char *app, const char *line); /* ENCH_LOG_DIR/<app>.log */
// kernel-внутренний доступ без lock-проверок (acc_login читает .pass.__)
i32 fs_raw_read(const char *path, void *buf, usize cap);
// правила имён и сборка путей (используют драйверы и shell)
i32 fs_name_hidden(const char *name);
i32 fs_name_locked(const char *name);
usize fs_path_join(char *out, usize cap, const char *dir, const char *name);
void fs_init(struct limine_file *tarball);

// ---- account (account/account.c) ----
// Хранение: ENCH_ACC_DIR/<логин>/.pass.__ (FNV-1a 64 с солью-логином).
// Первый запуск: аккаунтов нет => shell предлагает создание логина+пароля.
i32 acc_count(void);
i32 acc_create(const char *login, const char *pass);
i32 acc_login(const char *login, const char *pass);
i32 acc_list(char *out, usize cap);
const char *acc_current(void);
void acc_set_current(const char *login);

// ---- драйверы .b_d (drive/drive.c) ----
i32 drv_load(const char *path);
i32 drv_register(struct ench_drv *d);
i32 drv_list(char *out, usize cap);
struct ench_drv *drv_get(const char *name);
i32 drv_call(const char *name, u32 cmd, u64 arg);

// ---- модуль drive.mke (drive/mke.c, секция .enchmke) ----
void mke_init(void);

// ---- initd (initd/initd.c): *.einit = load/start/include/echo/sleep/# ----
void initd_run(void);

// ---- elf (elf/elfload.c) ----
i32 elf_load_ring3(const void *img, usize size, u64 *entry, u64 *stack_top);
// ET_REL (.mke/.b_d/ring0-приложения): вернуть адрес sym ("ench_mke_init",
// "ench_drv_init", "main"); символы против ksyms ядра + загруженных единиц
void *elf_rel_load(const void *buf, usize len, const char *sym, i32 *err);
void *elf_rel_sym(const char *name);
i32 elf_rel_unload_last(void);

// ---- динамический ассемблер (asmexec/asmexec.c) ----
i32 asm_run_file(const char *path, i64 *retval);
i32 asm_list(char *out, usize cap);
void asm_dump_last(void); /* OssTrace_AsmDump */

// ---- syscall (syscall/syscall.c) ----
void syscall_init(void);

// ---- power ----
void power_reboot(void);
void power_halt(void) __attribute__((noreturn));
u64 uptime_ms(void);
void rtc_read(struct ench_rtc *out);
