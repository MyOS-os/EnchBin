/* include/ench/mm.h — PMM/VMM/heap (документация).
 * Реализация: kernel/src/mm/mm.c; объявления — в ench/kernel.h
 * (pmm_init/pmm_alloc/pmm_free/pmm_free_kb/pmm_total_kb/vmm_init/vmm_map/
 * kmalloc/kfree/krealloc).
 * PMM: список свободных страниц по limine-memmap (usable), 4К на страницу.
 * VMM: 4-уровневые таблицы Limine, vmm_map(vaddr,paddr,flags) в текущее
 *   адресное пространство: flags bit0=W, bit1=U, bit2=NX off.
 * Heap: страничный аллокатор с заголовком (krealloc совместим с kmalloc).
 * Адреса: ядро — higher half 0xFFFFFFFF80000000 (kernel.ld), HHDM от Limine;
 * ring3-образы — 0x400000.., стек 0x70000000 (см. elf/elfload.c). */
#ifndef ENCH_MM_H
#define ENCH_MM_H
#include <ench/kernel.h>
#endif
