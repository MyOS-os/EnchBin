#pragma once
/* include/ench/abi.h — единый ABI EnchBin OS (ядро, .mke-модули, .b_d-драйверы,
 * приложения ring0 Platform/Apps и ring3 User/apps).
 * Единственный источник истины: номера syscalls 0..26, коды ошибок, флаги и
 * структуры, передаваемые через границу колец. */
#include <stdint.h>
#include <stddef.h>

#define ENCH_ABI_VERSION   0x00010000u /* v0.1.0 First Light */
#define ENCH_ABI_NAME      "EnchBin"

/* ---------- syscalls 0..26 ---------- */
#define SYS_read            0
#define SYS_write           1
#define SYS_open            2
#define SYS_close           3
#define SYS_lseek           4
#define SYS_unlink          5
#define SYS_getpid          6
#define SYS_yield           7
#define SYS_spawn           8
#define SYS_exec            9
#define SYS_exit           10
#define SYS_waitpid        11
#define SYS_sleep          12
#define SYS_brk            13
#define SYS_mmap           14
#define SYS_getkey         15
#define SYS_console_claim  16
#define SYS_console_release 17
#define SYS_console_exitcode 18
#define SYS_ps             19
#define SYS_kill           20
#define SYS_acc            21
#define SYS_setweight      22
#define SYS_setrt          23
#define SYS_runasm         24
#define SYS_drvctl         25
#define SYS_fsinfo         26
#define SYS_COUNT          27

/* коды ошибок (совместимы с духом POSIX, минимум для err-протокола shell) */
#define EOK     0
#define EPERM   1
#define ENOENT  2
#define EINVAL  3
#define ENOMEM  4
#define EACCES  5
#define EEXIST  6
#define ENOSYS  7
#define EIO     8
#define EBADF   9
#define ENOTDIR 10
#define EISDIR  11
#define ERR     99 /* универсальный 'err' для shell */

/* флаги узлов ФС (SYS_fsinfo / ls --a / cat-протокол) */
#define F_DIR    1u /* каталог */
#define F_HIDDEN 2u /* '__'-суффикс (lock): не виден без ls --a */
#define F_SECURE 4u /* аккаунт-материал: cat -> err */

/* open-флаги */
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_CREATE 4
#define O_APPEND 8
#define O_TRUNC  16

/* ACC-операции SYS_acc */
#define ACC_CREATE 1 /* (login, pass) */
#define ACC_LOGIN  2
#define ACC_LOGOUT 3
#define ACC_LIST   4
#define ACC_CURRENT 5

/* SYS_setrt */
#define RT_OFF 0
#define RT_ON  1

/* SYS_console_exitcode — J4: код завершения выводится одной строкой с новой строки */

/* ---------- структуры ABI ---------- */

struct ench_ps_ent {
    int64_t  pid, ppid;
    uint32_t state;      /* 0 unused,1 ready,2 running,3 blocked,4 zombie */
    uint32_t rt;         /* real-time поток */
    int64_t  weight;     /* вес кода (SYS_setweight) */
    char     name[16];
    int64_t  code_size;  /* размер ELF — вес при запуске */
};
struct ench_ps {
    int64_t count;
    struct ench_ps_ent e[32];
};

struct ench_fsinfo {
    uint32_t flags;      /* F_DIR / F_HIDDEN / F_SECURE */
    uint32_t pad;
    int64_t  size;
    char     name[64];
};

struct ench_acc_ent { char login[32]; };
struct ench_acc_list {
    int64_t count;
    int64_t logged_in;   /* индекс текущего или -1 */
    struct ench_acc_ent e[32];
};

struct ench_drv_ent {
    char     name[24];
    uint32_t kind;       /* DRV_KEYBOARD / DRV_TTY_ANSI / DRV_FS_COMPAT / DRV_MISC */
    uint32_t pad;
};
struct ench_drv_list {
    int64_t count;
    struct ench_drv_ent e[32];
};

struct ench_drvctl {
    int64_t op;          /* DRVCTL_* */
    char    name[24];    /* имя драйвера или пусто */
    int64_t a, b;
    void*   buf;
    uint64_t len;
    int64_t ret;         /* сюда пишется результат */
};

#define DRVCTL_LIST     1 /* заполнить ench_drv_list */
#define DRVCTL_IOCTL    2 /* вызвать ioctl драйвера name */
#define DRVCTL_LOAD     3 /* load .b_d по path в a (char*) */

/* SYS_runasm: динамический исполнитель ассемблера */
struct ench_runasm {
    const void* code;    /* машинный код, собранный runasm'ом */
    uint64_t    len;
    int64_t     result;  /* возвращённое значение */
    int64_t     err;     /* EOK/EACCES/EINVAL */
};

/* ---------- J4: протокол консоли ----------
 * 1) программа делает SYS_console_claim — вывод программы занимает консоль;
 * 2) печатает через SYS_write(1,...);
 * 3) самым последним действием вызывает SYS_console_exitcode(code) —
 *    код завершения выводится ЕДИНСТВЕННОЙ строкой с новой строки. */

/* ---------- соглашение имен ----------
 * Platform/Apps      — ring0-приложения (директный API ядра)
 * Platform/MyOS      — GUI-замашки (gui/gfx), тоже ring0
 * User/apps          — ring3-приложения (только через libench)
 * __-суффикс файла   — lock: скрыт без ls --a, cat -> err
 * .-файлы            — скрыты без ls --a
 */
