/* include/ench/drive.h — ABI модуля .mke (документация).
 * Реализация загрузчика: kernel/src/drive/mke.c; низкоуровневая загрузка
 * reloc-ELF — kernel/src/elf/elfload.c (elf_rel_load, объявления в kernel.h).
 *
 * Модуль drive.mke: reloc ELF, собирается ld -r из
 * kernel/modules/drive/src/drive_mod.c, встраивается в ядро через секцию
 * .enchmke (builder: objcopy --rename-section .data=.enchmke; границы —
 * символы ench_mke_blob_start/end в scripts/link/kernel.ld). При загрузке
 * ядра mke_init() резолвит ENCH_MKE_SYMBOL ("ench_mke_init") и вызывает его.
 * Роль drive.mke — активация подсистемы драйверов; загрузка .b_d — через
 * init.d_ (load) или SYS_drvctl LOAD. */
#ifndef ENCH_DRIVE_H
#define ENCH_DRIVE_H
#include <ench/abi.h>
#endif
