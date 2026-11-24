// kernel/src/mm/mm.c — PMM (список свободных страниц), VMM (простоход PML4)
// и kernel heap. Работает поверх раскладки Limine (HHDM + cr3).
#include <ench/kernel.h>

#define PG 4096ull
#define NX_CLEAR (~0x8000000000000000ull)

// ---- PMM ----
static u64 *pfree_head;
static usize pfree_cnt, ptotal_kb;
static u64 hhdm;

static inline void *ph2v(u64 p) { return (void *)(p + hhdm); }
static inline u64 v2ph(void *v) { return (u64)v - hhdm; }

void pmm_init(struct limine_memmap_response *mm) {
    pfree_head = 0; pfree_cnt = 0; ptotal_kb = 0;
    if (!mm) return;
    for (u64 i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) continue;
        ptotal_kb += (usize)(e->length / 1024);
        // пропускаем первые 1 MiB региона (real-area/BIOS следы)
        u64 base = e->base < 0x100000 ? 0x100000 : e->base;
        for (u64 p = ENCH_ALIGN_UP(base, PG); p + PG <= e->base + e->length; p += PG) {
            u64 *n = ph2v(p);
            *n = (u64)pfree_head;
            pfree_head = n;
            pfree_cnt++;
        }
    }
}

void *pmm_alloc(void) {
    if (!pfree_head) return 0;
    u64 p = v2ph(pfree_head);
    pfree_head = (u64 *)*pfree_head;
    pfree_cnt--;
    ench_memset(ph2v(p), 0, PG);
    return (void *)p;
}

void pmm_free(void *page) {
    if (!page) return;
    u64 *v = ph2v((u64)page);
    *v = (u64)pfree_head;
    pfree_head = v;
    pfree_cnt++;
}

usize pmm_free_kb(void)  { return pfree_cnt * 4; }
usize pmm_total_kb(void) { return ptotal_kb; }

// ---- VMM ----
void vmm_init(u64 hhdm_off) { hhdm = hhdm_off; }

static u64 *tab(u64 idx, u64 *parent, u64 flags) {
    u64 *e = parent + idx;
    if (*e & 1) return ph2v(*e & 0x000FFFFFFFFFF000ull);
    u64 p = (u64)pmm_alloc();
    if (!p) return 0;
    ench_memset(ph2v(p), 0, PG);
    *e = p | 1 | flags;
    return ph2v(p);
}

void vmm_map(u64 vaddr, u64 paddr, u64 flags) {
    u64 cr3; __asm__ volatile("movq %%cr3, %0" : "=r"(cr3));
    u64 *pml4 = ph2v(cr3 & 0x000FFFFFFFFFF000ull);
    u64 *pdpt = tab((vaddr >> 39) & 511, pml4, 0);
    if (!pdpt) return;
    u64 *pd   = tab((vaddr >> 30) & 511, pdpt, 0);
    if (!pd) return;
    u64 *pt   = tab((vaddr >> 21) & 511, pd, 0);
    if (!pt) return;
    u64 e = paddr | 1 | flags;
    e &= NX_CLEAR;
    pt[(vaddr >> 12) & 511] = e;
    __asm__ volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}

// ---- kernel heap: страничный аллокатор с заголовком ----
struct khd { u32 magic; u32 pages; };

void *kmalloc(usize n) {
    if (!n) return 0;
    usize pages = (ENCH_ALIGN_UP(n + sizeof(struct khd), PG)) / PG;
    u64 p = (u64)pmm_alloc();
    if (!p) return 0;
    for (usize i = 1; i < pages; i++) {
        u64 q = (u64)pmm_alloc();
        if (!q) return 0;
        if (q != p + i * PG) { /* фрагментация: вернём лишние */
            for (usize j = i; j < pages; j++) ;
            pages = i;
            break;
        }
    }
    struct khd *h = ph2v(p);
    h->magic = 0xE0CB1B0C; h->pages = (u32)pages;
    return h + 1;
}

void kfree(void *p) {
    if (!p) return;
    struct khd *h = (struct khd *)p - 1;
    if (h->magic != 0xE0CB1B0C) return;
    u64 base = v2ph(h);
    for (u32 i = 0; i < h->pages; i++) pmm_free((void *)(base + i * PG));
}

void *krealloc(void *p, usize n) {
    void *q = kmalloc(n);
    if (p && q) {
        struct khd *h = (struct khd *)p - 1;
        usize old = (usize)h->pages * PG - sizeof(struct khd);
        ench_memcpy(q, p, ENCH_MIN(old, n));
        kfree(p);
    }
    return q;
}
