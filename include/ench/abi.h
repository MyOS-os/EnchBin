/* include/ench/abi.h — единый ABI EnchBin OS (ядро, .mke, .b_d, ring3).
 * Полнот rewrite: здесь одни syscall-номера (0..26) для ядра (syscall.c),
 * libench (stdlib/c) и Rust/D/asm-stdlib. Ничего лишнего, demo-прототипов нет. */
#ifndef ENCH_ABI_H
#define ENCH_ABI_H
#include <stdint.h>
#include <stddef.h>

#define ENCH_ABI_VERSION 0x00010000u /* v0.1.0 First Light */

/* ---------- syscalls 0..26 (единый список) ---------- */
#define SYS_argstr          0 /* -> u64 ptr на строку аргументов (NULL) */
#define SYS_console_write   1 /* a=buf b=len -> записать в консоль */
#define SYS_console_readc   2 /* -> код клавиши или -1 */
#define SYS_open            3 /* a=path b=flags(0 ro/1 rw/2 rw|create) -> fd */
#define SYS_close           4
#define SYS_read            5
#define SYS_write           6
#define SYS_seek            7
#define SYS_stat            8 /* a=fd buf=ench_fs_ent */
#define SYS_list            9 /* a=dirpath buf=ench_fs_ent[] b=cap -> count */
#define SYS_mkdir          10 /* a=dirpath b=name */
#define SYS_create         11
#define SYS_remove         12
#define SYS_spawn          13 /* a=path b=args(или 0) -> tid */
#define SYS_wait           14 /* a=tid -> exit code */
#define SYS_exit           15
#define SYS_sleep          16 /* a=ms */
#define SYS_gettime        17 /* -> ms с загрузки */
#define SYS_getpid         18
#define SYS_yield          19
#define SYS_setweight      20 /* a=tid b=weight */
#define SYS_setrt          21 /* a=tid b=0/1 */
#define SYS_drive_call     22 /* a=cmd b/c/Buf: модуль drive (drive.mke) */
#define SYS_drv_call       23 /* a=name b=cmd c/d/Buf: сервис драйвера */
#define SYS_gui_open       24 /* x,y,w,h,title -> win id */
#define SYS_gui_draw       25 /* id,cmd,a,b,c,d */
#define SYS_gui_poll       26 /* id -> клавиша/клик или -1 */
#define SYS_COUNT          27

/* коды ошибок (negative errno; shell печатает err при ret<0) */
#define ENCH_EOK     0
#define ENCH_EPERM  (-1)
#define ENCH_ENOENT (-2)
#define ENCH_ENOMEM (-12)
#define ENCH_EACCES (-13)
#define ENCH_EEXIST (-17)
#define ENCH_ENOTDIR (-20)
#define ENCH_EISDIR (-21)
#define ENCH_EINVAL (-22)
#define ENCH_ENOSYS (-38)

/* флаги записей ФС */
#define ENCH_F_DIR   1u
#define ENCH_F_HIDDEN 2u /* имя с '.' — не видно без ls --a */
#define ENCH_F_LOCK  4u  /* имя с '__' — lock: чтение->err, запись/удаление запрещены */

/* J4: протокол консоли. Пока выполняется приложение, консоль принадлежит ему;
 * вывод ядра подавлен; когда приложение завершается, ядро выводит ОДНУ строку
 * — код завершения — всегда с новой строки. */

/* ---------- структуры через границу колец ---------- */
struct ench_fs_ent {
    char     name[64];
    uint32_t kind;   /* ENCH_F_DIR или 0 */
    uint32_t flags;  /* ENCH_F_* */
    uint64_t size;
};

struct ench_unitinfo {
    char     name[32];
    uint32_t kind;   /* ENCH_MKE_* / ENCH_DRV_* / ENCH_APP_* */
    uint32_t pad;
    uint64_t size;
    uint64_t base;
};

/* ---------- модуль .mke (единственный — drive.mke, секция .enchmke) ---------- */
#define ENCH_MKE_MAGIC 0x00454B4Du /* "MKE\0" */
#define ENCH_MKE_ABI   1
struct ench_mdesc {
    uint32_t magic;       /* ENCH_MKE_MAGIC */
    uint32_t abi;         /* ENCH_MKE_ABI */
    const char *name;
    int  (*init)(void);
    int  (*fini)(void);
    int  (*call)(uint32_t cmd, int64_t a, int64_t b, void *buf, uint64_t len);
};
/* команды call модуля drive */
#define DRIVE_LOAD   1 /* a=path(inside kernel), b=len -> 0/err */
#define DRIVE_LIST   2 /* buf=ench_unitinfo[], b=cap -> count */
#define DRIVE_UNLOAD 3 /* a=индекс единицы */

/* ---------- драйвер/приложение ring0 (.b_d, reloc ELF, символ ench_b_d) ---------- */
#define ENCH_BD_MAGIC 0x00444245u /* "EBD\0" */
#define ENCH_BD_ABI   1
#define ENCH_DRV_CHAR  1
#define ENCH_DRV_UNIX  2
#define ENCH_DRV_USERD 3
#define ENCH_APP_RING0 4
struct ench_bdesc {
    uint32_t magic;       /* ENCH_BD_MAGIC */
    uint32_t kind;        /* ENCH_DRV_* / ENCH_APP_RING0 */
    const char *name;
    const char *abi;
    int  (*init)(void);   /* драйвер: регистрация; ring0-приложение: main */
    int  (*fini)(void);
    int  (*call)(uint32_t cmd, int64_t a, int64_t b, void *buf, uint64_t len);
    int64_t weight_hint;  /* 0 = авто: вес кода = размер образа (страницы 4K) */
};

/* ---------- ELF64 (минимум для загрузчиков) ---------- */
struct ench_ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum;
    uint16_t e_shentsize, e_shnum, e_shstrndx;
};
struct ench_phdr {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
};
struct ench_shdr {
    uint32_t sh_name, sh_type;
    uint64_t sh_flags, sh_addr, sh_offset, sh_size;
    uint32_t sh_link, sh_info;
    uint64_t sh_addralign, sh_entsize;
};
struct ench_sym {
    uint32_t st_name;
    uint8_t  st_info, st_other;
    uint16_t st_shndx;
    uint64_t st_value, st_size;
};
struct ench_rela {
    uint64_t r_offset;
    uint32_t r_type, r_sym;
    int64_t  r_addend;
};
#define ENCH_ET_EXEC 2
#define ENCH_ET_REL  1
#define ENCH_EM_X64  62
#define ENCH_PT_LOAD 1
#define ENCH_SHT_SYMTAB 2
#define ENCH_SHT_RELA   4
#define ENCH_R_64       1
#define ENCH_R_PC32     2
#define ENCH_R_PLT32    4
#define ENCH_R_GOTPCREL 9
#define ENCH_R_32S      11
#define ENCH_R_REX_GOTPCRELX 42

#endif
