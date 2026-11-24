// kernel/src/drive/mke.c — загрузчик модулей ядра .mke.
// Единственный модуль — drive.mke: reloc ELF с экспортом ENCH_MKE_SYMBOL
// ("ench_mke_init"), встроен в ядро при сборке через секцию .enchmke
// (builder: objcopy --rename-section .data=.enchmke).
#include <ench/kernel.h>
#include <ench/elf.h>
#include <ench/osstrace.h>

// границы blob-а модуля (scripts/link/kernel.ld: .enchmke)
extern u8 ench_mke_blob_start[];
extern u8 ench_mke_blob_end[];

void mke_init(void) {
    usize len = (usize)(ench_mke_blob_end - ench_mke_blob_start);
    if (len < sizeof(struct elf64_ehdr)) {
        kprintf("mke: drive.mke не встроен (соберите модуль: bin/builder module)\n");
        return;
    }
    i32 err;
    void *fn = elf_rel_load(ench_mke_blob_start, len, ENCH_MKE_SYMBOL, &err);
    if (!fn) {
        kprintf("mke: drive.mke: загрузка не удалась (err=%d)\n", err);
        return;
    }
    OssTrace_Trace("mke: drive.mke %zu байт", len);
    ((void (*)(void))fn)();
}
