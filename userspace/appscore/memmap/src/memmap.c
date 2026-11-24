// userspace/appscore/memmap/src/memmap.c — ring0-приложение (ET_REL "main").
// Дамп limine-memmap: base/length/type каждой записи + итоги PMM.
#include <ench/kernel.h>

static void out(const char *s) { console_app_write(s, ench_strlen(s)); }
static void outln(const char *s) { out(s); out("\n"); }

static void hex64(u64 v, char *b) {
    b[0] = '0'; b[1] = 'x';
    int started = 0;
    usize o = 2;
    for (int i = 15; i >= 0; i--) {
        u8 d = (u8)((v >> (i * 4)) & 0xF);
        if (!d && !started && i) continue;
        started = 1;
        b[o++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
    }
    if (!started) b[o++] = '0';
    b[o] = 0;
}

static void dec64(u64 v, char *b) {
    char t[24];
    usize n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
    for (usize i = 0; i < n; i++) b[i] = t[n - 1 - i];
    b[n] = 0;
}

static const char *mm_type(u64 t) {
    switch (t) {
    case LIMINE_MEMMAP_USABLE:                 return "usable";
    case LIMINE_MEMMAP_RESERVED:               return "reserved";
    case LIMINE_MEMMAP_ACPI_RECLAIMABLE:       return "acpi-recl";
    case LIMINE_MEMMAP_ACPI_NVS:               return "acpi-nvs";
    case LIMINE_MEMMAP_BAD_MEMORY:             return "bad";
    case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "bootloader";
    case LIMINE_MEMMAP_KERNEL_AND_MODULES:     return "kernel/mod";
    case LIMINE_MEMMAP_FRAMEBUFFER:            return "framebuffer";
    default:                                   return "?";
    }
}

void main(const char *args) {
    (void)args;
    outln("--- memmap ---");
    struct limine_memmap_response *mm = ench_boot_memmap();
    if (!mm) { outln("memmap otvetstviya net"); return; }
    char b1[20], b2[20];
    for (u64 i = 0; i < mm->entry_count; i++) {
        struct limine_memmap_entry *e = mm->entries[i];
        hex64(e->base, b1);
        dec64(e->length, b2);
        out(b1);
        out(" +");
        out(b2);
        out("  ");
        outln(mm_type(e->type));
    }
    out("pmm: vsego ");
    dec64(pmm_total_kb(), b1);
    out(b1);
    out(" KB, svobodno ");
    dec64(pmm_free_kb(), b2);
    out(b2);
    outln(" KB");
    hex64(ench_boot_hhdm(), b1);
    out("hhdm offset: ");
    outln(b1);
}
