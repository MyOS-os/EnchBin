#pragma once
/* include/ench/elf.h — минимальные структуры ELF64 для загрузчиков EnchBin.
 * Объявления функций загрузки — в ench/kernel.h:
 *   elf_load_ring3(img, size, &entry, &stack_top) — ET_EXEC ring3 (User/apps);
 *   elf_rel_load(buf, len, sym, &err)             — ET_REL (.mke drive.mke,
 *       .b_d-драйверы, ring0-приложения Platform/Apps и Platform/MyOS):
 *       секции раскладываются в арену у конца ядра, RELA применяются против
 *       таблицы экспорта ядра (ksyms) и уже загруженных единиц, возвращается
 *       адрес sym ("ench_mke_init"/"ench_drv_init"/"main");
 *   elf_rel_sym(name)                             — поиск символа по всем единицам;
 *   elf_rel_unload_last()                         — выгрузка последней единицы (devctl).
 * Конвенция экспорта: .mke -> ENCH_MKE_SYMBOL, .b_d -> "ench_drv_init",
 * ring0-приложения -> "main". */
#ifndef ENCH_ELF_H
#define ENCH_ELF_H
#include <stdint.h>
#include <stddef.h>

#define ELF_MAGIC 0x464C457Fu /* "\x7fELF" little endian u32 */

#define ET_REL  1
#define ET_EXEC 2
#define EM_X86_64 62

#define PT_LOAD 1

#define SHT_NOBITS  8
#define SHT_SYMTAB  2
#define SHT_RELA    4
#define SHN_UNDEF   0
#define SHF_ALLOC   0x2
#define SHF_WRITE   0x1

#define R_X86_64_64    1
#define R_X86_64_PC32  2
#define R_X86_64_PLT32 4
#define R_X86_64_32    10
#define R_X86_64_32S   11

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

#endif
