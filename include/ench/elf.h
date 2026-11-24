#pragma once
/* include/ench/elf.h — минимальный ELF64 для загрузчиков EnchBin:
 * 1) ET_EXEC ring3-приложения (User/apps, база 0x400000) — в собственные
 *    таблицы страниц задачи;
 * 2) ET_REL (.mke-модуль drive.mke, .b_d-драйверы, ring0-приложения
 *    Platform/Apps и Platform/MyOS) — загрузка в kernel-арену с релокациями
 *    против symtab самого ядра + уже загруженных единиц. */
#include <stdint.h>
#include <stddef.h>

#define ELF_MAGIC 0x464C457Fu /* "\x7fELF" little endian u32 */

#define ET_EXEC 2
#define ET_REL  1
#define EM_X86_64 62

#define PT_LOAD 1

#define SHT_SYMTAB 2
#define SHT_RELA   4
#define SHN_UNDEF  0

#define R_X86_64_64    1
#define R_X86_64_PC32  2
#define R_X86_64_PLT32 4

struct elf64_ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum;
    uint16_t e_shentsize, e_shnum, e_shstrndx;
};
struct elf64_phdr {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
};
struct elf64_shdr {
    uint32_t sh_name, sh_type;
    uint64_t sh_flags, sh_addr, sh_offset, sh_size;
    uint32_t sh_link, sh_info;
    uint64_t sh_addralign, sh_entsize;
};
struct elf64_sym {
    uint32_t st_name;
    uint8_t  st_info, st_other;
    uint16_t st_shndx;
    uint64_t st_value, st_size;
};
struct elf64_rela {
    uint64_t r_offset;
    uint32_t r_type, r_sym;
    int64_t  r_addend;
};

/* загрузка ring3 ET_EXEC: раскладывает сегменты в pml4 задачи, возвращает entry */
struct task;
int  elf_exec_load(struct task *t, const char *path, uint64_t *entry_out);

/* загрузка ET_REL: копирует секции в свежую память, применяет RELA
 * (R_X86_64_64/PC32/PLT32) против elf_rel_sym(); возвращает адрес символа "init"
 * (для .mke/.b_d) или "main" (для ring0-приложений), NULL при ошибке */
void *elf_rel_load(const void *buf, size_t len, const char *name, int *err);

/* поиск символа: symtab ядра (линкер сохраняет symtab), затем symtab'ы всех
 * загруженных ET_REL единиц — так ring0-приложения видят API ядра и друг друга */
void *elf_rel_sym(const char *name);

/* выгрузить последнюю единицу (для devctl) */
int  elf_rel_unload_last(void);
