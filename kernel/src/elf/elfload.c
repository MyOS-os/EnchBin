// kernel/src/elf/elfload.c — загрузка ELF: ET_EXEC ring3 (User/apps, база
// 0x400000) и ET_REL (.mke drive.mke, .b_d-драйверы, ring0-приложения
// Platform/Apps и Platform/MyOS). ET_REL-секции раскладываются в арену сразу
// после конца ядра (верхние 2 ГиБ => PC32-релокации валидны), символы
// резолвятся против ksyms ядра и уже загруженных единиц.
#include <ench/kernel.h>
#include <ench/kcpu.h>
#include <ench/elf.h>
#include <ench/osstrace.h>

#define PG 4096ull
#define USER_STACK_TOP   0x70000000ull
#define USER_STACK_PAGES 16
#define USER_MEM_LIMIT   0x70000000ull

// ---- арена единиц: VA сразу после конца ядра ----
extern u8 _ench_kernel_end[];
static u64 arena_next;

static void *arena_alloc(usize size, usize align) {
    if (align < 16) align = 16;
    if (!arena_next) arena_next = ENCH_ALIGN_UP((u64)_ench_kernel_end, PG);
    u64 va = ENCH_ALIGN_UP(arena_next, align);
    usize pages = ENCH_ALIGN_UP(size ? size : 1, PG) / PG;
    for (usize i = 0; i < pages; i++) {
        void *pg = pmm_alloc();
        if (!pg) return 0;
        vmm_map(va + i * PG, (u64)pg, 5); // W + NX off (RWX: toy-арена единиц)
    }
    arena_next = va + pages * PG;
    return (void *)va;
}

// ---- таблица экспорта ядра (ksyms) ----
#define KSYM_LIST(X) \
    X(kprintf) X(kputs) X(kputc) X(kclear) X(ksetattr) X(kscroll) X(kfont_glyph) \
    X(console_app_write) X(console_exit_line) X(console_acquire) X(console_release) \
    X(kinput_push) X(kinput_pop) X(kserial_write) X(kvformat) \
    X(kmalloc) X(kfree) X(krealloc) X(pmm_alloc) X(pmm_free) X(pmm_free_kb) X(pmm_total_kb) X(vmm_map) \
    X(fs_open) X(fs_read) X(fs_write) X(fs_close) X(fs_stat) X(fs_mkdir) X(fs_unlink) \
    X(fs_listdir) X(fs_chdir) X(fs_getcwd) X(fs_log_write) X(fs_raw_read) \
    X(fs_name_hidden) X(fs_name_locked) X(fs_path_join) \
    X(acc_count) X(acc_create) X(acc_login) X(acc_list) X(acc_current) X(acc_set_current) \
    X(drv_load) X(drv_register) X(drv_list) X(drv_get) X(drv_call) \
    X(mke_init) X(initd_run) \
    X(elf_load_ring3) X(elf_rel_load) X(elf_rel_sym) X(elf_rel_unload_last) \
    X(asm_run_file) X(asm_list) X(asm_dump_last) \
    X(task_spawn_elf) X(task_spawn_kth) X(task_yield) X(task_setweight) X(task_setrt) \
    X(task_kill) X(task_list) X(task_weight_for_size) X(task_reap) X(task_current_pid) \
    X(task_exit) X(task_exit_core) \
    X(power_reboot) X(power_halt) X(uptime_ms) X(rtc_read) X(syscall_init) X(scheduler_tick) \
    X(cpu_set_kbd_hook)

// все имена объявлены в kernel.h/kcpu.h — адреса берём напрямую
static const struct { const char *name; void *addr; } ksyms[] = {
#define KSYM(n) { #n, (void *)&(n) },
    KSYM_LIST(KSYM)
#undef KSYM
};

static void *ksym_find(const char *name) {
    for (usize i = 0; i < ENCH_ARRAY_SIZE(ksyms); i++)
        if (ench_strcmp(ksyms[i].name, name) == 0) return ksyms[i].addr;
    return 0;
}

// ---- загруженные ET_REL-единицы ----
#define UNIT_MAX 16
#define UNIT_SH  48
struct unit {
    u8 used;
    char name[16];
    u32 shnum;
    u64 secmem[UNIT_SH];
    struct elf64_sym *symtab;
    u32 nsyms;
    const char *strtab;
};
static struct unit units[UNIT_MAX];

static void *resolve_global(const char *name, struct unit *self) {
    for (int i = 0; i < UNIT_MAX; i++) {
        struct unit *u = &units[i];
        if (!u->used || u == self) continue;
        for (u32 j = 0; j < u->nsyms; j++) {
            const struct elf64_sym *s = &u->symtab[j];
            if (s->st_shndx == SHN_UNDEF || !s->st_name) continue;
            if (ench_strcmp(u->strtab + s->st_name, name) == 0)
                return (void *)(u->secmem[s->st_shndx] + s->st_value);
        }
    }
    return ksym_find(name);
}

void *elf_rel_sym(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < UNIT_MAX; i++) {
        struct unit *u = &units[i];
        if (!u->used) continue;
        for (u32 j = 0; j < u->nsyms; j++) {
            const struct elf64_sym *s = &u->symtab[j];
            if (s->st_shndx == SHN_UNDEF || !s->st_name) continue;
            if (ench_strcmp(u->strtab + s->st_name, name) == 0)
                return (void *)(u->secmem[s->st_shndx] + s->st_value);
        }
    }
    return ksym_find(name);
}

i32 elf_rel_unload_last(void) {
    for (int i = UNIT_MAX - 1; i >= 0; i--)
        if (units[i].used) { units[i].used = 0; return ENCH_EOK; }
    return ENCH_ENOENT;
}

// ---- загрузка ET_REL: вернуть адрес want ("ench_mke_init"/"ench_drv_init"/"main") ----
void *elf_rel_load(const void *buf, usize len, const char *want, i32 *err) {
    if (err) *err = ENCH_EINVAL;
    if (!buf || !want || len < sizeof(struct elf64_ehdr)) return 0;
    const struct elf64_ehdr *eh = buf;
    if (*(const u32 *)eh->e_ident != ELF_MAGIC) return 0;
    if (eh->e_type != ET_REL || eh->e_machine != EM_X86_64) return 0;
    if (!eh->e_shoff || !eh->e_shnum || eh->e_shnum > UNIT_SH ||
        eh->e_shentsize < sizeof(struct elf64_shdr)) return 0;
    if (eh->e_shoff + (usize)eh->e_shnum * eh->e_shentsize > len) return 0;
    const struct elf64_shdr *sh = (const void *)((const u8 *)buf + eh->e_shoff);

    // symtab + strtab
    const struct elf64_shdr *symtab_sh = 0;
    for (u32 i = 0; i < eh->e_shnum; i++)
        if (sh[i].sh_type == SHT_SYMTAB) { symtab_sh = &sh[i]; break; }
    if (!symtab_sh || symtab_sh->sh_link >= eh->e_shnum) return 0;
    u32 nsyms = (u32)(symtab_sh->sh_size / sizeof(struct elf64_sym));
    const struct elf64_shdr *strtab_sh = &sh[symtab_sh->sh_link];

    // слот единицы
    struct unit *u = 0;
    for (int i = 0; i < UNIT_MAX; i++) if (!units[i].used) { u = &units[i]; break; }
    if (!u) { if (err) *err = ENCH_ENOMEM; return 0; }
    ench_memset(u, 0, sizeof *u);
    u->shnum = eh->e_shnum;

    // ALLOC-секции в арену
    for (u32 i = 0; i < eh->e_shnum; i++) {
        if (!(sh[i].sh_flags & SHF_ALLOC) || sh[i].sh_size == 0) continue;
        void *mem = arena_alloc((usize)sh[i].sh_size, (usize)sh[i].sh_addralign);
        if (!mem) { if (err) *err = ENCH_ENOMEM; return 0; }
        if (sh[i].sh_type == SHT_NOBITS)
            ench_memset(mem, 0, (usize)sh[i].sh_size);
        else {
            if (sh[i].sh_offset + sh[i].sh_size > len) { if (err) *err = ENCH_EINVAL; return 0; }
            ench_memcpy(mem, (const u8 *)buf + sh[i].sh_offset, (usize)sh[i].sh_size);
        }
        u->secmem[i] = (u64)mem;
    }

    // symtab/strtab копируем в арену (buf может быть временным)
    struct elf64_sym *sym = arena_alloc((usize)nsyms * sizeof(struct elf64_sym), 8);
    char *str = arena_alloc((usize)strtab_sh->sh_size, 1);
    if (!sym || !str) { if (err) *err = ENCH_ENOMEM; return 0; }
    ench_memcpy(sym, (const void *)((const u8 *)buf + symtab_sh->sh_offset),
                (usize)nsyms * sizeof(struct elf64_sym));
    ench_memcpy(str, (const void *)((const u8 *)buf + strtab_sh->sh_offset),
                (usize)strtab_sh->sh_size);
    u->symtab = sym; u->nsyms = nsyms; u->strtab = str;

    // RELA-релокации
    for (u32 i = 0; i < eh->e_shnum; i++) {
        if (sh[i].sh_type != SHT_RELA) continue;
        if (sh[i].sh_info >= u->shnum || !u->secmem[sh[i].sh_info]) continue;
        u8 *target = (u8 *)u->secmem[sh[i].sh_info];
        const struct elf64_rela *rel = (const void *)((const u8 *)buf + sh[i].sh_offset);
        usize nrel = (usize)(sh[i].sh_size / sizeof(struct elf64_rela));
        for (usize j = 0; j < nrel; j++) {
            const struct elf64_rela *r = &rel[j];
            if (r->r_offset >= sh[sh[i].sh_info].sh_size) { if (err) *err = ENCH_EINVAL; return 0; }
            if (r->r_sym >= nsyms) { if (err) *err = ENCH_EINVAL; return 0; }
            const struct elf64_sym *s = &sym[r->r_sym];
            u64 S;
            if (s->st_shndx == SHN_UNDEF || s->st_shndx >= u->shnum) {
                void *g = resolve_global(str + s->st_name, u);
                if (!g) { if (err) *err = ENCH_ENOENT; return 0; } // неразрешённый символ
                S = (u64)g;
            } else {
                S = u->secmem[s->st_shndx] + s->st_value;
            }
            u64 P = (u64)target + r->r_offset;
            i64 A = r->r_addend;
            switch (r->r_type) {
            case R_X86_64_64:    *(u64 *)P = S + (u64)A; break;
            case R_X86_64_32S:   *(i32 *)P = (i32)(S + A); break;
            case R_X86_64_32:    *(u32 *)P = (u32)((u64)(S + A)); break;
            case R_X86_64_PC32:
            case R_X86_64_PLT32: *(i32 *)P = (i32)(S + A - (i64)P); break;
            default: if (err) *err = ENCH_EUNKNOWN; return 0;
            }
        }
    }

    // искомый символ
    void *fn = 0;
    for (u32 i = 0; i < nsyms; i++) {
        if (sym[i].st_shndx == SHN_UNDEF || sym[i].st_shndx >= u->shnum) continue;
        if (ench_strcmp(str + sym[i].st_name, want) == 0) {
            fn = (void *)(u->secmem[sym[i].st_shndx] + sym[i].st_value);
            break;
        }
    }
    if (!fn) { if (err) *err = ENCH_ENOENT; return 0; }

    for (int i = 0; want[i] && i < 15; i++) u->name[i] = want[i];
    u->used = 1;
    OssTrace_Trace("elf: unit '%s' loaded", want);
    if (err) *err = ENCH_EOK;
    return fn;
}

// ---- загрузка ET_EXEC ring3: раскладка в текущее адресное пространство ----
i32 elf_load_ring3(const void *img, usize size, u64 *entry, u64 *stack_top) {
    if (!img || size < sizeof(struct elf64_ehdr)) return ENCH_EINVAL;
    const struct elf64_ehdr *eh = img;
    if (*(const u32 *)eh->e_ident != ELF_MAGIC) return ENCH_EINVAL;
    if (eh->e_type != ET_EXEC || eh->e_machine != EM_X86_64) return ENCH_EINVAL;
    if (!eh->e_phoff || !eh->e_phnum || eh->e_phnum > 16 ||
        eh->e_phentsize < sizeof(struct elf64_phdr)) return ENCH_EINVAL;
    if (eh->e_phoff + (usize)eh->e_phnum * eh->e_phentsize > size) return ENCH_EINVAL;

    // стек ring3: [top-64KiB, top)
    u64 top = USER_STACK_TOP;
    for (u32 i = 0; i < USER_STACK_PAGES; i++) {
        void *pg = pmm_alloc();
        if (!pg) return ENCH_ENOMEM;
        vmm_map(top - (USER_STACK_PAGES - i) * PG, (u64)pg, 3); // W|U
    }

    const struct elf64_phdr *ph = (const void *)((const u8 *)img + eh->e_phoff);
    for (u32 i = 0; i < eh->e_phnum; i++) {
        const struct elf64_phdr *p = &ph[i];
        if (p->p_type != PT_LOAD || p->p_memsz == 0) continue;
        if (p->p_vaddr + p->p_memsz > USER_MEM_LIMIT) return ENCH_EINVAL;
        if (p->p_offset + p->p_filesz > size) return ENCH_EINVAL;
        u64 va = p->p_vaddr & ~(PG - 1);
        u64 end = p->p_vaddr + p->p_memsz;
        u64 flags = (p->p_flags & PF_W) ? 3 : 2; // W|U : R|U
        for (u64 q = va; q < end; q += PG) {
            void *pg = pmm_alloc();
            if (!pg) return ENCH_ENOMEM;
            vmm_map(q, (u64)pg, flags);
        }
        if (p->p_filesz)
            ench_memcpy((void *)p->p_vaddr, (const u8 *)img + p->p_offset, (usize)p->p_filesz);
        if (p->p_memsz > p->p_filesz)
            ench_memset((u8 *)p->p_vaddr + p->p_filesz, 0, (usize)(p->p_memsz - p->p_filesz));
    }
    *entry = eh->e_entry;
    *stack_top = top;
    OssTrace_Trace("elf: exec entry=%p", eh->e_entry);
    return ENCH_EOK;
}
