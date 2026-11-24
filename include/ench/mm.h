#pragma once
/* include/ench/mm.h — PMM (bitmap по limine-memmap), VMM (4-уровневые таблицы,
 * страницы 2M), HHDM и heap (kmalloc/kfree). */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* HHDM */
extern uint64_t ench_hhdm_off;
static inline void *hhdm(void *phys) { return (void *)((uint64_t)phys + ench_hhdm_off); }
static inline void *phys_of(void *hhdmp) { return (void *)((uint64_t)hhdmp - ench_hhdm_off); }

/* PMM */
void     pmm_init(void);
void    *pmm_alloc(void);        /* 4K физическая страница */
void     pmm_free(void *page);
uint64_t pmm_free_pages(void);
void     pmm_reserve_range(uint64_t base, uint64_t len);

/* VMM */
#define PTE_PRESENT (1ull << 0)
#define PTE_WRITE   (1ull << 1)
#define PTE_USER    (1ull << 2)
#define PTE_NOEXEC  (1ull << 63)

void      vmm_init(void);
uint64_t *vmm_kernel_pml4(void);
uint64_t *vmm_new_pml4(void);                       /* копия kernel-раскладки */
void      vmm_map(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags);
void      vmm_map_user_range(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, size_t len);
void      vmm_switch(uint64_t *pml4);

/* heap (kernel) */
void  heap_init(void);
void *kmalloc(size_t n);
void  kfree(void *p);
void *kcalloc(size_t n, size_t sz);
void *krealloc(void *p, size_t n);

/* драйверная арена: область памяти для .b_d/.mke-единиц (инициализируется
 * init.d_ при загрузке драйверов) */
void  drv_arena_init(size_t bytes);
void *drv_arena_alloc(size_t n, size_t align);
size_t drv_arena_used(void);
size_t drv_arena_total(void);

/* база загруженного ядра (для symtab-поиска) */
extern uint64_t ench_kernel_base;

/* статистика для sysinfo/memmap */
void mm_stats(uint64_t out[4]); /* free_pages, total_pages, heap_used, arena_used */
