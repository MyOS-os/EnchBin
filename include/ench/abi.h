// include/ench/abi.h — системный ABI EnchBin: syscalls 0..26, дескрипторы
// драйверов (.b_d), модуля drive.mke, форматы .easm и пути системы.
#pragma once

#include <ench/base.h>

// ---- Syscalls (rax = номер, SysV: rdi,rsi,rdx,r10,r8,r9; ошибка => отрицательный errno) ----
#define SYS_read      0
#define SYS_write     1
#define SYS_open      2
#define SYS_close     3
#define SYS_stat      4
#define SYS_mkdir     5
#define SYS_unlink    6
#define SYS_listdir   7
#define SYS_spawn     8
#define SYS_exit      9
#define SYS_wait      10
#define SYS_getpid    11
#define SYS_yield     12
#define SYS_sleep     13
#define SYS_setweight 14 /* вес потока для weighted round-robin */
#define SYS_setrt     15 /* реальный поток: не прерывается по тику */
#define SYS_tasklist  16 /* снимок таблицы задач (taskman) */
#define SYS_sbrk      17
#define SYS_chdir     18
#define SYS_getcwd    19
#define SYS_acc_create 20
#define SYS_acc_login  21
#define SYS_whoami     22
#define SYS_asmrun     23 /* динамический ассемблер: файлы .easm */
#define SYS_drvctl     24 /* реестр драйверов .b_d */
#define SYS_time       25 /* RTC */
#define SYS_powerctl   26 /* 1=reboot, 2=halt */

#define ENCH_FD_STDIN  0
#define ENCH_FD_STDOUT 1
#define ENCH_FD_STDERR 2

// ---- Протокол консоли J4 ----
// 1) пока приложение активно, вывод приложения монополен: kernel-лог идёт
//    на COM1 и в filelog, а не на экран;
// 2) после завершения приложения ядро печатает последнюю строку на НОВОЙ строке
//    и только её: "exit <code>".
#define ENCH_EXIT_LINE_FMT "exit %d"

// ---- Реестр драйверов (.b_d = reloc ELF, грузятся по SYS_drvctl / init.d_) ----
struct ench_drv {
    const char *name;   /* "keyboard", "ansi", "msdos", "unix_lin" */
    const char *type;   /* "input" | "term" | "fs" | "compat" */
    int (*init)(void);
    int (*write)(const void *buf, usize len);
    int (*read)(void *buf, usize len);
    int (*ioctl)(u32 cmd, u64 arg);
};

#define ENCH_DRVCTL_LIST 1 /* перечислить реестр */
#define ENCH_DRVCTL_LOAD 2 /* загрузить .b_d по пути (rdi=path) */
#define ENCH_DRVCTL_CALL 3 /* вызвать ioctl драйвера (rsi=name, rdx=cmd, r10=arg) */

// ---- Модуль ядра drive.mke (встраивается в секцию .enchmke) ----
// reloc ELF, обязан экспортировать символ void ench_mke_init(void).
#define ENCH_MKE_SYMBOL "ench_mke_init"

// ---- Динамический ассемблер (.easm) ----
#define ENCH_EASM_DIR  "/Platform/data/asm"
#define ENCH_EASM_MAX  4096 /* лимит сгенерированного машинного кода, байт */

// ---- Пути системы ----
#define ENCH_ACC_DIR   "/Platform/data/system/.data-hash-user"
#define ENCH_INITD_DIR "/Platform/data/system/init.d_"
#define ENCH_LOG_DIR   "/Platform/data/system/log"
#define ENCH_DRIVERS_SYS "/Platform/ench.bin/system/drivers"
#define ENCH_DRIVERS_USR "/Platform/ench.bin/system/drivers/userd"
#define ENCH_DRIVE_UNIX  "/Platform/ench.bin/system/drive"
#define ENCH_LOCK_SUFFIX ".__"   /* суффикс lock-файлов: скрыты без ls --a */

// ---- RTC ----
struct ench_rtc { u32 year, mon, day, hour, min, sec; };

// ---- Снимок задач (SYS_tasklist) ----
struct ench_taskinfo {
    i32 pid;
    char name[16];
    u8  state;   /* ENCH_TASK_* */
    u8  rt;
    u32 weight;
};
enum { ENCH_TASK_RUN = 1, ENCH_TASK_READY, ENCH_TASK_SLEEP, ENCH_TASK_BLOCK, ENCH_TASK_DEAD };

// ---- Метаданные файла (SYS_stat / fs_stat / e_stat) ----
struct ench_fstat {
    i64 size;
    u8  is_dir;
    u8  locked;   /* суффикс .__: чтение -> ENCH_ELOCKED */
    u8  hidden;   /* скрытое имя (точка в начале или суффикс .__) */
    char name[56];
};

// ---- Режимы открытия (SYS_open / fs_open / e_open) ----
enum { ENCH_O_READ = 0, ENCH_O_WRITE = 1, ENCH_O_CREATE = 2, ENCH_O_APPEND = 4 };

// ---- powerctl ----
#define ENCH_POWER_REBOOT 1
#define ENCH_POWER_HALT   2
