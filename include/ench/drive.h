/* include/ench/drive.h — ABI модуля .mke и .b_d (единственная реализация
 * загрузчика: kernel/src/drive.c). Инициализация цепочки:
 *   init.d_/autostart.einit:  module drive  ->  драйверы .b_d  ->  приложения. */
#ifndef ENCH_DRIVE_H
#define ENCH_DRIVE_H
#include <stdint.h>
#include <stddef.h>
#include <ench/abi.h>

/* загрузка reloc-ELF (внутренние примитивы ядра; .mke использует их через ksyms) */
struct ench_bdesc;
struct ench_mdesc;
long ench_kload_reloc(const void *blob, size_t size, const char *want_sym,
                      void **desc_out, char *name_out, size_t name_cap);
void ench_kunit_removed_last(void);

/* команды SYS_drive_call (= call модуля drive) */
/* DRIVE_LOAD/DRIVE_LIST/DRIVE_UNLOAD — см. abi.h */

#endif
